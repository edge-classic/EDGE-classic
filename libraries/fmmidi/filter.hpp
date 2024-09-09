#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <complex>
#include <map>
#include <vector>

namespace filter
{
/*
typedef long int_least32_t;
*/

// �v�Z
inline int pow2(int x)
{
    return 1 << x;
}

int log2(int n);
int log2_ceil(int n);

// �����t�[���G�ϊ�
void fft(std::complex<double> dst[], const std::complex<double> src[], int n);
void ifft(std::complex<double> dst[], const std::complex<double> src[], int n);

// ���֐�
void hanning_window(double dst[], const double src[], std::size_t n);

// FIR�t�B���^
class finite_impulse_response
{
  public:
    finite_impulse_response();
    void set_impulse_response(const double *h, std::size_t length);
    void set_impulse_response(const std::vector<double> &h)
    {
        set_impulse_response(&h[0], h.size());
    }
    void apply(int_least32_t *out, const int_least32_t *in, std::size_t length,
               std::size_t stride = sizeof(int_least32_t));

  private:
    std::vector<int_least32_t> h;
    std::vector<int_least32_t> buffer;
    std::size_t                pos;
    std::size_t                hlen;
};

// �t�B���^�쐬
void compute_equalizer_fir(double *h, std::size_t length, double rate, const std::map<double, double> &gains);
} // namespace filter