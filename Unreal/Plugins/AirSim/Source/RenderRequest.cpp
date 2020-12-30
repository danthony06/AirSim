#include "RenderRequest.h"
#include "TextureResource.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Async/TaskGraphInterfaces.h"
#include "ImageUtils.h"
#include "Containers/UnrealString.h"
#include <thread>
#include <chrono>
#include "common/common_utils/BufferPool.h"
#include "AirBlueprintLib.h"
#include "Async/Async.h"

RenderRequest::RenderRequest(BufferPool *buffer_pool) : buffer_pool_(buffer_pool)
{}

RenderRequest::~RenderRequest()
{}

void RenderRequest::FastScreenshot()
{
<<<<<<< HEAD
    UAirBlueprintLib::RunCommandOnGameThread([this]() {
        fast_cap_done_ = false;
        fast_rt_resource_ = fast_param_.render_component->TextureTarget->GameThread_GetRenderTargetResource();
        fast_param_.render_component->CaptureScene();
        ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)([this](FRHICommandListImmediate& RHICmdList)
        {
            this->RenderThreadScreenshotTask(this->latest_result_);
=======
    //TODO: is below really needed?
    for (unsigned int i = 0; i < req_size; ++i) {
        results.push_back(std::make_shared<RenderResult>());

        if (!params[i]->pixels_as_float)
            results[i]->bmp.Reset();
        else
            results[i]->bmp_float.Reset();
        results[i]->time_stamp = 0;
    }

    //make sure we are not on the rendering thread
    CheckNotBlockedOnRenderThread();

    if (use_safe_method) {
        for (unsigned int i = 0; i < req_size; ++i) {
            //TODO: below doesn't work right now because it must be running in game thread
            FIntPoint img_size;
            if (!params[i]->pixels_as_float) {
                //below is documented method but more expensive because it forces flush
                FTextureRenderTargetResource* rt_resource = params[i]->render_target->GameThread_GetRenderTargetResource();
                auto flags = setupRenderResource(rt_resource, params[i].get(), results[i].get(), img_size);
                rt_resource->ReadPixels(results[i]->bmp, flags);
            }
            else {
                FTextureRenderTargetResource* rt_resource = params[i]->render_target->GetRenderTargetResource();
                setupRenderResource(rt_resource, params[i].get(), results[i].get(), img_size);
                rt_resource->ReadFloat16Pixels(results[i]->bmp_float);
            }
        }
    }
    else {
        //wait for render thread to pick up our task
        params_ = params;
        results_ = results.data();
        req_size_ = req_size;

        // Queue up the task of querying camera pose in the game thread and synchronizing render thread with camera pose
        AsyncTask(ENamedThreads::GameThread, [this]() {
            check(IsInGameThread());

            saved_DisableWorldRendering_ = game_viewport_->bDisableWorldRendering;
            game_viewport_->bDisableWorldRendering = 0;
            end_draw_handle_ = game_viewport_->OnEndDraw().AddLambda([this] {
                check(IsInGameThread());

                // capture CameraPose for this frame
                query_camera_pose_cb_();

                // The completion is called immeidately after GameThread sends the
                // rendering commands to RenderThread. Hence, our ExecuteTask will
                // execute *immediately* after RenderThread renders the scene!
                RenderRequest* This = this;
                ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
                [This](FRHICommandListImmediate& RHICmdList)
                {
                    This->ExecuteTask();
                });

                game_viewport_->bDisableWorldRendering = saved_DisableWorldRendering_;

                assert(end_draw_handle_.IsValid());
                game_viewport_->OnEndDraw().Remove(end_draw_handle_);
            });

            // while we're still on GameThread, enqueue request for capture the scene!
            for (unsigned int i = 0; i < req_size_; ++i) {
                params_[i]->render_component->CaptureSceneDeferred();
            }
>>>>>>> e9f95a7772820c63c8d31e166d557aaf0c150f76
        });
    }, false);

    //Try to wait just long enough for the render thread to finish the capture.
    //Start with a long wait, then check for completion more frequently.
    //TODO: Optimize these numbers.
    for (int best_guess_cap_time_microseconds = 500; !fast_cap_done_; best_guess_cap_time_microseconds = 200)
        std::this_thread::sleep_for(std::chrono::microseconds(best_guess_cap_time_microseconds));
}

void RenderRequest::RenderThreadScreenshotTask(RenderRequest::RenderResult &result)
{
    FRHITexture2D *fast_cap_texture = fast_rt_resource_->TextureRHI->GetTexture2D();
    EPixelFormat pixelFormat = fast_cap_texture->GetFormat();
    uint32 width = fast_cap_texture->GetSizeX();
    uint32 height = fast_cap_texture->GetSizeY();
    uint32 stride;
    auto *src = (const unsigned char*)RHILockTexture2D(fast_cap_texture, 0, RLM_ReadOnly, stride, false); // needs to be on render thread

    result.time_stamp = msr::airlib::ClockFactory::get()->nowNanos();
    result.pixels = buffer_pool_->GetBufferExactSize(height*stride);
    result.stride = stride;
    result.width = width;
    result.height = height;
    result.pixel_format = pixelFormat;

    if (src)
		FMemory::BigBlockMemcpy(latest_result_.pixels->data(), src, height * stride);

    RHIUnlockTexture2D(fast_cap_texture, 0, false);

    fast_cap_done_ = true;
}
