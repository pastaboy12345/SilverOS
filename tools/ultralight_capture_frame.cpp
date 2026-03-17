#include <AppCore/Platform.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/Ultralight.h>
#include <Ultralight/View.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Surface.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace ultralight;

static const char* kCaptureHtml =
    "<!doctype html>"
    "<html><head><meta charset='utf-8'>"
    "<style>"
    "body{margin:0;background:linear-gradient(160deg,#0b1322,#183552 60%,#1e5669);"
    "font-family:sans-serif;color:#eef3ff;display:grid;place-items:center;min-height:100vh}"
    ".card{width:min(92vw,900px);padding:34px;border-radius:26px;"
    "background:rgba(7,14,24,.82);border:1px solid rgba(145,194,255,.3)}"
    "h1{margin:0 0 10px;font-size:48px}"
    "p{margin:0;font-size:21px;line-height:1.5;color:#dbe4f6}"
    "</style></head><body><section class='card'><h1>SilverOS + Ultralight SDK</h1>"
    "<p>Rendered by vendored Ultralight SDK compatibility pipeline.</p>"
    "</section></body></html>";

class CaptureApp : public LoadListener, public Logger {
 public:
  CaptureApp(uint32_t width, uint32_t height, const std::string& sdk_root)
      : width_(width), height_(height) {
    Config config;
    config.resource_path_prefix = "resources/";
    config.bitmap_alignment = 0;

    auto& platform = Platform::instance();
    platform.set_config(config);
    platform.set_font_loader(GetPlatformFontLoader());
    platform.set_file_system(GetPlatformFileSystem(String(sdk_root.c_str())));
    platform.set_logger(this);

    renderer_ = Renderer::Create();
    if (!renderer_) {
      throw std::runtime_error("Failed to create Ultralight renderer");
    }

    ViewConfig view_config;
    view_config.is_accelerated = false;
    view_config.initial_device_scale = 1.0;
    view_config.enable_images = true;
    view_config.enable_javascript = true;

    view_ = renderer_->CreateView(width_, height_, view_config, nullptr);
    if (!view_) {
      throw std::runtime_error("Failed to create Ultralight view");
    }

    view_->set_load_listener(this);
    view_->LoadHTML(String(kCaptureHtml), String("file:///silveros_ultralight_capture.html"), false);
    view_->Focus();
  }

  void OnFinishLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame,
                       const String& url) override {
    (void)caller;
    (void)frame_id;
    (void)url;
    if (is_main_frame) {
      done_ = true;
    }
  }

  void OnFailLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame,
                     const String& url, const String& description, const String& error_domain,
                     int error_code) override {
    (void)caller;
    (void)frame_id;
    (void)url;
    (void)error_domain;
    (void)error_code;
    if (is_main_frame) {
      done_ = true;
      failed_ = true;
      failure_message_ = description.utf8().data();
    }
  }

  void LogMessage(LogLevel log_level, const String& message) override {
    const char* level = "INFO";
    if (log_level == LogLevel::Error) {
      level = "ERROR";
    } else if (log_level == LogLevel::Warning) {
      level = "WARN";
    }
    std::cerr << "[Ultralight/" << level << "] " << message.utf8().data() << "\n";
  }

  bool Capture(std::vector<uint32_t>& pixels, uint32_t& out_w, uint32_t& out_h) {
    for (int i = 0; i < 800 && !done_; ++i) {
      renderer_->Update();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (failed_) {
      std::cerr << "Ultralight capture failed: " << failure_message_ << "\n";
      return false;
    }

    renderer_->RefreshDisplay(0);
    renderer_->Render();

    Surface* surface = view_->surface();
    if (!surface) {
      std::cerr << "Ultralight view has no CPU surface.\n";
      return false;
    }

    BitmapSurface* bitmap_surface = static_cast<BitmapSurface*>(surface);
    RefPtr<Bitmap> bitmap = bitmap_surface->bitmap();
    if (!bitmap) {
      std::cerr << "Ultralight bitmap surface is null.\n";
      return false;
    }

    const uint32_t bw = bitmap->width();
    const uint32_t bh = bitmap->height();
    const uint32_t row_bytes = bitmap->row_bytes();
    const uint8_t* src = static_cast<const uint8_t*>(bitmap->LockPixels());
    if (!src) {
      std::cerr << "Unable to lock Ultralight bitmap pixels.\n";
      return false;
    }

    pixels.resize((size_t)bw * (size_t)bh);
    for (uint32_t y = 0; y < bh; ++y) {
      std::memcpy(&pixels[(size_t)y * bw], src + (size_t)y * row_bytes, (size_t)bw * sizeof(uint32_t));
    }
    bitmap->UnlockPixels();

    surface->ClearDirtyBounds();
    out_w = bw;
    out_h = bh;
    return true;
  }

 private:
  RefPtr<Renderer> renderer_;
  RefPtr<View> view_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  bool done_ = false;
  bool failed_ = false;
  std::string failure_message_;
};

static bool WriteHeader(const std::string& path, uint32_t width, uint32_t height,
                        const std::vector<uint32_t>& pixels) {
  std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
  if (!out.good()) {
    std::cerr << "Failed to open output header: " << path << "\n";
    return false;
  }

  out << "#ifndef SILVEROS_GENERATED_ULTRALIGHT_FRAME_H\n";
  out << "#define SILVEROS_GENERATED_ULTRALIGHT_FRAME_H\n\n";
  out << "#include <stdint.h>\n";
  out << "#include <stddef.h>\n\n";
  out << "static const uint32_t kUltralightFrameWidth = " << width << "u;\n";
  out << "static const uint32_t kUltralightFrameHeight = " << height << "u;\n";
  out << "static const size_t kUltralightFramePixelCount = " << pixels.size() << "u;\n";
  out << "static const uint32_t kUltralightFramePixels[" << pixels.size() << "] = {\n";

  for (size_t i = 0; i < pixels.size(); ++i) {
    if (i % 8 == 0) {
      out << "  ";
    }

    out << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << pixels[i] << "u";
    if (i + 1 < pixels.size()) {
      out << ",";
    }
    out << " ";

    if (i % 8 == 7) {
      out << "\n";
    }
  }

  if (pixels.size() % 8 != 0) {
    out << "\n";
  }

  out << "};\n\n";
  out << "#endif\n";
  out.flush();
  return out.good();
}

int main(int argc, char** argv) {
  std::string out_header;
  std::string sdk_root;
  uint32_t width = 256;
  uint32_t height = 144;

  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <output_header> <sdk_root> [width] [height]\n";
    return 2;
  }

  out_header = argv[1];
  sdk_root = argv[2];
  if (argc >= 4) {
    width = (uint32_t)std::strtoul(argv[3], nullptr, 10);
  }
  if (argc >= 5) {
    height = (uint32_t)std::strtoul(argv[4], nullptr, 10);
  }

  if (width == 0 || height == 0) {
    std::cerr << "Invalid frame size.\n";
    return 2;
  }

  try {
    CaptureApp app(width, height, sdk_root);
    std::vector<uint32_t> pixels;
    uint32_t out_w = 0;
    uint32_t out_h = 0;
    if (!app.Capture(pixels, out_w, out_h)) {
      return 1;
    }

    if (!WriteHeader(out_header, out_w, out_h, pixels)) {
      return 1;
    }
  } catch (const std::exception& ex) {
    std::cerr << "Capture error: " << ex.what() << "\n";
    return 1;
  }

  std::cout << "Generated Ultralight frame asset: " << out_header << "\n";
  return 0;
}
