#include "xtmwebgpu.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
/* use wayland */
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#endif
#include <GLFW/glfw3native.h>

/* --- surface creation (vendored from glfw3webgpu) --- */

WGPUSurface xtm_wgpu_create_surface(WGPUInstance instance,
                                     GLFWwindow *window) {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WGPUSurfaceSourceWindowsHWND source = {
        .chain = {.sType = WGPUSType_SurfaceSourceWindowsHWND},
        .hinstance = hinstance,
        .hwnd = hwnd,
    };
    WGPUSurfaceDescriptor desc = {
        .nextInChain = (const WGPUChainedStruct *)&source,
    };
    return wgpuInstanceCreateSurface(instance, &desc);

#elif defined(__APPLE__)
    NSWindow *nswindow = glfwGetCocoaWindow(window);
    NSView *view = [nswindow contentView];
    [view setWantsLayer:YES];
    CAMetalLayer *layer = [CAMetalLayer layer];
    [view setLayer:layer];
    WGPUSurfaceSourceMetalLayer source = {
        .chain = {.sType = WGPUSType_SurfaceSourceMetalLayer},
        .layer = (__bridge void *)layer,
    };
    WGPUSurfaceDescriptor desc = {
        .nextInChain = (const WGPUChainedStruct *)&source,
    };
    return wgpuInstanceCreateSurface(instance, &desc);

#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    struct wl_display *display = glfwGetWaylandDisplay();
    struct wl_surface *wl_surface = glfwGetWaylandWindow(window);
    WGPUSurfaceSourceWaylandSurface source = {
        .chain = {.sType = WGPUSType_SurfaceSourceWaylandSurface},
        .display = display,
        .surface = wl_surface,
    };
    WGPUSurfaceDescriptor desc = {
        .nextInChain = (const WGPUChainedStruct *)&source,
    };
    return wgpuInstanceCreateSurface(instance, &desc);

#else
    Display *x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    WGPUSurfaceSourceXlibWindow source = {
        .chain = {.sType = WGPUSType_SurfaceSourceXlibWindow},
        .display = x11_display,
        .window = (uint64_t)x11_window,
    };
    WGPUSurfaceDescriptor desc = {
        .nextInChain = (const WGPUChainedStruct *)&source,
    };
    return wgpuInstanceCreateSurface(instance, &desc);
#endif
}

/* --- synchronous adapter request --- */

typedef struct {
    WGPUAdapter adapter;
    int done;
} AdapterResult;

static void on_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                        WGPUStringView message, void *userdata1,
                        void *userdata2) {
    AdapterResult *result = (AdapterResult *)userdata1;
    (void)userdata2;
    if (status == WGPURequestAdapterStatus_Success) {
        result->adapter = adapter;
    } else {
        fprintf(stderr, "wgpu adapter request failed: %.*s\n",
                (int)message.length, message.data);
        result->adapter = NULL;
    }
    result->done = 1;
}

WGPUAdapter xtm_wgpu_request_adapter(WGPUInstance instance,
                                      WGPUSurface surface) {
    WGPURequestAdapterOptions opts = {
        .compatibleSurface = surface,
        .powerPreference = WGPUPowerPreference_HighPerformance,
    };
    WGPURequestAdapterCallbackInfo cb = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_adapter,
    };
    AdapterResult result = {.adapter = NULL, .done = 0};
    cb.userdata1 = &result;
    wgpuInstanceRequestAdapter(instance, &opts, cb);
    while (!result.done) {
        wgpuInstanceProcessEvents(instance);
    }
    return result.adapter;
}

/* --- synchronous device request --- */

typedef struct {
    WGPUDevice device;
    int done;
} DeviceResult;

static void on_device(WGPURequestDeviceStatus status, WGPUDevice device,
                       WGPUStringView message, void *userdata1,
                       void *userdata2) {
    DeviceResult *result = (DeviceResult *)userdata1;
    (void)userdata2;
    if (status == WGPURequestDeviceStatus_Success) {
        result->device = device;
    } else {
        fprintf(stderr, "wgpu device request failed: %.*s\n",
                (int)message.length, message.data);
        result->device = NULL;
    }
    result->done = 1;
}

WGPUDevice xtm_wgpu_request_device(WGPUInstance instance,
                                    WGPUAdapter adapter) {
    WGPUDeviceDescriptor desc = {0};
    desc.defaultQueue.label = (WGPUStringView){.data = "default", .length = 7};
    WGPURequestDeviceCallbackInfo cb = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
    };
    cb.callback = on_device;
    DeviceResult result = {.device = NULL, .done = 0};
    cb.userdata1 = &result;
    wgpuAdapterRequestDevice(adapter, &desc, cb);
    while (!result.done) {
        wgpuInstanceProcessEvents(instance);
    }
    return result.device;
}

/* --- surface configuration --- */

void xtm_wgpu_configure_surface(WGPUSurface surface, WGPUDevice device,
                                 uint32_t format, uint32_t width,
                                 uint32_t height) {
    WGPUSurfaceConfiguration config = {
        .device = device,
        .format = (WGPUTextureFormat)format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = width,
        .height = height,
        .presentMode = WGPUPresentMode_Fifo,
    };
    wgpuSurfaceConfigure(surface, &config);
}

/* --- preferred surface format --- */

uint32_t xtm_wgpu_surface_format(WGPUSurface surface, WGPUAdapter adapter) {
    WGPUSurfaceCapabilities caps = {0};
    wgpuSurfaceGetCapabilities(surface, adapter, &caps);
    uint32_t fmt = (uint32_t)caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return fmt;
}

/* --- shader module --- */

WGPUShaderModule xtm_wgpu_create_shader(WGPUDevice device, const char *wgsl) {
    WGPUShaderSourceWGSL wgsl_source = {
        .chain = {.sType = WGPUSType_ShaderSourceWGSL},
        .code = {.data = wgsl, .length = strlen(wgsl)},
    };
    WGPUShaderModuleDescriptor desc = {
        .nextInChain = (const WGPUChainedStruct *)&wgsl_source,
    };
    return wgpuDeviceCreateShaderModule(device, &desc);
}

/* --- render pipeline --- */

WGPURenderPipeline xtm_wgpu_create_pipeline(WGPUDevice device,
                                              WGPUShaderModule shader,
                                              uint32_t format,
                                              const char *vs_entry,
                                              const char *fs_entry) {
    WGPUColorTargetState color_target = {
        .format = (WGPUTextureFormat)format,
        .writeMask = WGPUColorWriteMask_All,
    };
    WGPUFragmentState fragment = {
        .module = shader,
        .entryPoint = {.data = fs_entry, .length = strlen(fs_entry)},
        .targetCount = 1,
        .targets = &color_target,
    };
    WGPURenderPipelineDescriptor desc = {
        .vertex =
            {
                .module = shader,
                .entryPoint = {.data = vs_entry, .length = strlen(vs_entry)},
            },
        .primitive =
            {
                .topology = WGPUPrimitiveTopology_TriangleList,
            },
        .multisample =
            {
                .count = 1,
                .mask = 0xFFFFFFFF,
            },
        .fragment = &fragment,
    };
    return wgpuDeviceCreateRenderPipeline(device, &desc);
}

/* --- begin frame --- */

void xtm_wgpu_begin_frame(WGPUSurface surface, WGPUDevice device, double r,
                            double g, double b, double a,
                            WGPUCommandEncoder *out_encoder,
                            WGPURenderPassEncoder *out_pass,
                            WGPUTextureView *out_view) {
    WGPUSurfaceTexture st;
    wgpuSurfaceGetCurrentTexture(surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        fprintf(stderr, "wgpu: failed to get surface texture (status %d)\n",
                st.status);
        *out_encoder = NULL;
        *out_pass = NULL;
        *out_view = NULL;
        return;
    }

    WGPUTextureViewDescriptor view_desc = {0};
    WGPUTextureView view = wgpuTextureCreateView(st.texture, &view_desc);

    WGPUCommandEncoderDescriptor enc_desc = {0};
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device, &enc_desc);

    WGPURenderPassColorAttachment color_att = {
        .view = view,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {.r = r, .g = g, .b = b, .a = a},
    };
    WGPURenderPassDescriptor rp_desc = {
        .colorAttachmentCount = 1,
        .colorAttachments = &color_att,
    };
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &rp_desc);

    *out_encoder = encoder;
    *out_pass = pass;
    *out_view = view;
}

/* --- end frame --- */

void xtm_wgpu_end_frame(WGPUSurface surface, WGPUQueue queue,
                          WGPUCommandEncoder encoder,
                          WGPURenderPassEncoder pass, WGPUTextureView view) {
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cb_desc = {0};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cb_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuSurfacePresent(surface);
    wgpuTextureViewRelease(view);
}
