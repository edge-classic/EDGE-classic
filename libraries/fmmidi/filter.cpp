#include "filter.hpp"

#include <algorithm>
#include <cassert>

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace filter{
    // n��2���Ƃ���ΐ�(�؂�̂�)
    int log2(int n)
    {
        assert(n >= 1);
        int x = 0;
        while(n){
            ++x;
            n >>= 1;
        }
        return x - 1;
    }
    // n��2���Ƃ���ΐ�(�؂�グ)
    int log2_ceil(int n)
    {
        assert(n >= 1);
        int x = 0;
        while(n > 1){
            ++x;
            n >>= 1;
        }
        return x;
    }

    // �����t�[���G�ϊ�
    void fft(complex<double> dst[], const complex<double> src[], int n)
    {
        assert(n >= 1);
        int size = pow2(n);
        vector<int> reversal(size);
        for(int i = 0; i < size; ++i){
            int a = i, b = 0;
            for(int j = 0; j < n; ++j){
                b <<= 1;
                b |= a & 1;
                a >>= 1;
            }
            reversal[i] = b;
        }
        vector< complex<double> > x0(&src[0], &src[size]);
        vector< complex<double> > x1(size);
        complex<double> A = exp(complex<double>(0, -2 * M_PI / size));
        for(int r = 1; r <= n; ++r){
            int n_r = n - r;
            int bit = pow2(n_r);
            for(int i = 0; i < size; ++i){
                int s = i & ~((bit << 1) - 1);  // s = i / (bit * 2) * (bit * 2);
                s = reversal[s];
                s <<= n_r;                      // s *= bit;
                const complex<double>& src1 = x0[i & ~bit];
                const complex<double>& src2 = x0[i | bit];
                if(i & bit){
                    x1[i] = src1 - src2 * pow(A, s);
                }else{
                    x1[i] = src1 + src2 * pow(A, s);
                }
            }
            if(r < n){
                x0.swap(x1);
            }
        }
        for(int i = 0; i < size; ++i){
            dst[i] = x1[reversal[i]];
        }
    }

    // �t�����t�[���G�ϊ�
    void ifft(complex<double> dst[], const complex<double> src[], int n)
    {
        assert(n >= 1);
        int size = pow2(n);
        vector<int> reversal(size);
        for(int i = 0; i < size; ++i){
            int a = i, b = 0;
            for(int j = 0; j < n; ++j){
                b <<= 1;
                b |= a & 1;
                a >>= 1;
            }
            reversal[i] = b;
        }
        vector< complex<double> > x0(&src[0], &src[size]);
        vector< complex<double> > x1(size);
        complex<double> A = exp(complex<double>(0, 2 * M_PI / size));
        for(int r = 1; r <= n; ++r){
            int n_r = n - r;
            int bit = pow2(n_r);
            for(int i = 0; i < size; i++){
                int s = i & ~((bit << 1) - 1);  // s = i / (bit * 2) * (bit * 2);
                s = reversal[s];
                s <<= n_r;                      // s *= bit;
                const complex<double>& src1 = x0[i & ~bit];
                const complex<double>& src2 = x0[i | bit];
                if(i & bit){
                    x1[i] = src1 - src2 * pow(A, s);
                }else{
                    x1[i] = src1 + src2 * pow(A, s);
                }
            }
            if(r < n){
                x0.swap(x1);
            }
        }
        for(int i = 0; i < size; ++i){
            dst[i] = x1[reversal[i]] / static_cast<double>(size);
        }
    }

    // �n�j���O��
    void hanning_window(double dst[], const double src[], size_t n)
    {
        double t = 2 * M_PI / n;
        for(size_t i = 0; i < n; ++i){
            dst[i] = src[i] * (0.5 - 0.5 * cos(t * i));
        }
    }

    // FIR�R���X�g���N�^
    finite_impulse_response::finite_impulse_response()
    {
        buffer.resize(1);
        h.assign(1, 1);
        pos = 0;
        hlen = 1;
    }
    // FIR�W���ݒ�
    void finite_impulse_response::set_impulse_response(const double* h_, size_t length)
    {
        hlen = length;
        h.resize(pow2(log2_ceil(length)));
        for(size_t i = 0; i < length; ++i){
            h[i] = static_cast<long>(h_[i] * (1 << 12));
        }
        for(size_t i = length; i < h.size(); ++i){
            h[i] = 0;
        }
        while(hlen > 1 && h[hlen - 1] == 0){
            --hlen;
        }
        length = h.size();
        if(buffer.size() < length){
            size_t size = buffer.size();
            size_t d = length - size;
            buffer.resize(length);
            memmove(&buffer[pos + d], &buffer[pos], sizeof(buffer[0]) * (size - pos));
            memset(&buffer[pos], 0, sizeof(buffer[0]) * d);
        }
    }
    // FIR�t�B���^�K�p
    void finite_impulse_response::apply(int_least32_t* out, const int_least32_t* in, size_t length, std::size_t stride)
    {
        std::size_t buflenmask = buffer.size() - 1;
        while(length > 0){
            buffer[pos] = *in;
            pos = (pos + 1) & buflenmask;
            size_t offset = pos + buffer.size() - hlen;
            int_least32_t result = 0;
            for(size_t i = 0; i < hlen; ++i){
                result += h[i] * buffer[(offset + i) & buflenmask] >> 12;
            }
            *out = result;
            in = reinterpret_cast<const int_least32_t*>(reinterpret_cast<const char*>(in) + stride);
            out = reinterpret_cast<int_least32_t*>(reinterpret_cast<char*>(out) + stride);
            --length;
        }
    }

    // �C�R���C�UFIR�t�B���^�쐬
    void compute_equalizer_fir(double* h, std::size_t length, double rate, const std::map<double, double>& gains)
    {
        for(std::size_t i = 0; i < length; ++i){
            h[i] = 0;
        }
        if(gains.empty()){
            h[0] = 1;
        }else{
            int h_bits = log2(length);
            size_t length = pow2(h_bits);
            size_t half_length = length / 2;
            std::map<double, double> gain_bounds;
            std::map<double, double>::const_iterator i = gains.begin();
            gain_bounds[0] = i->second;
            for(;;){
                double fL = i->first;
                double gL = i->second;
                ++i;
                if(i == gains.end()){
                    break;
                }
                double fR = i->first;
                double gR = i->second;
                double log_fL = log(fL);
                double log_fR = log(fR);
                const int n = 16;
                for(int i = 0; i < n; ++i){
                    double ft = (i + 0.5) / n;
                    double f = exp(log_fL * (1 - ft) + log_fR * ft);
                    double gt = static_cast<double>(i) / n;
                    double g = gL * (1 - gt) + gR * gt;
                    gain_bounds[f] = g;
                }
            }
            double T = 1 / rate;
            for(size_t k = 0; k < half_length; ++k){
                double kT = k * T;
                double hk = 0;
                i = gain_bounds.begin();
                while(i != gain_bounds.end()){
                    double gain = i->second;
                    double f0 = i->first;
                    ++i;
                    double f1 = i == gain_bounds.end() ? rate / 2 : i->first;
                    double w0 = f0 * 2 * M_PI;
                    double w1 = f1 * 2 * M_PI;
                    if(k == 0){
                        hk += gain * (w1 - w0 + (-w0) - (-w1));
                    }else{
                        double w0kT = w0 * kT;
                        double w1kT = w1 * kT;
                        /*
                        hk += + gain * exp(complex<double>(0, w1kT)) / complex<double>(0, kT)
                              - gain * exp(complex<double>(0, w0kT)) / complex<double>(0, kT)
                              + gain * exp(complex<double>(0, -w0kT)) / complex<double>(0, kT)
                              - gain * exp(complex<double>(0, -w1kT)) / complex<double>(0, kT);
                        */
                        hk += gain * (sin(w1kT) - sin(w0kT)) * 2 / kT;
                    }
                }
                hk *= T / (2 * M_PI);
                h[half_length - 1 - k] = hk;
                h[half_length - 1 + k] = hk;
            }
        }
    }
}
