#include "sokol_images.h"

#include "epi.h"

static constexpr uint32_t kMaxSamplers = 256;

static sg_sampler                             samplers[kMaxSamplers];
static sg_sampler_desc                        sampler_descs[kMaxSamplers];
static int32_t                                num_samplers = 0;
static std::unordered_map<uint32_t, uint32_t> image_samplers;

void RegisterImageSampler(uint32_t imageId, sg_sampler_desc *desc)
{
    sg_sampler_desc *d = sampler_descs;
    int              i;
    for (i = 0; i < num_samplers; i++, d++)
    {
        if (!memcmp(d, desc, sizeof(sg_sampler_desc)))
        {
            image_samplers[imageId] = samplers[i].id;
            return;
        }
    }

    if (num_samplers == kMaxSamplers)
    {
        FatalError("Sampler overflow");
    }

    memcpy(&sampler_descs[num_samplers], desc, sizeof(sg_sampler_desc));
    samplers[num_samplers]  = sg_make_sampler(desc);
    image_samplers[imageId] = samplers[num_samplers].id;
    num_samplers++;
}

void GetImageSampler(uint32_t imageId, uint32_t *sampler_id)
{
    std::unordered_map<uint32_t, uint32_t>::const_iterator itr = image_samplers.find(imageId);

    if (itr == image_samplers.end())
    {
        FatalError("Unable to get image sampler");
    }

    *sampler_id = itr->second;
}

void InitImages()
{
    num_samplers = 0;
    EPI_CLEAR_MEMORY(samplers, sg_sampler, kMaxSamplers);
    EPI_CLEAR_MEMORY(sampler_descs, sg_sampler_desc, kMaxSamplers);
}

static std::vector<sg_image> deleted_images;

void DeleteImage(sg_image image)
{
    for (std::vector<sg_image>::iterator itr = deleted_images.begin(); itr != deleted_images.end(); itr++)
    {
        if (itr->id == image.id)
        {
            return;
        }
    }

    deleted_images.push_back(image);
}

void FinalizeDeletedImages()
{
    for (std::vector<sg_image>::iterator itr = deleted_images.begin(); itr != deleted_images.end(); itr++)
    {
        image_samplers.erase(image_samplers.find(itr->id));
        sg_destroy_image(*itr);
    }

    deleted_images.clear();
}
