#include "clilog.h"
#include "config.h"
#include "decafsdl.h"

#include <common/decaf_assert.h>
#include <common/log.h>
#include <common/platform_dir.h>
#include <libconfig/config_excmd.h>
#include <libconfig/config_toml.h>
#include <libdecaf/decaf.h>
#include <excmd.h>
#include <iostream>

std::shared_ptr<spdlog::logger>
gCliLog;

using namespace decaf::input;

static excmd::parser
getCommandLineParser()
{
   excmd::parser parser;
   using excmd::description;
   using excmd::optional;
   using excmd::default_value;
   using excmd::allowed;
   using excmd::value;

   parser.global_options()
      .add_option("v,version",
                  description { "Show version." })
      .add_option("h,help",
                  description { "Show help." });

   parser.add_command("help")
      .add_argument("help-command",
                    optional {},
                    value<std::string> {});

   auto frontend_options = parser.add_option_group("Frontend Options")
      .add_option("config",
                  description { "Specify path to configuration file." },
                  value<std::string> {})
      .add_option("force-sync",
                  description { "Force rendering to sync with gpu flips." })
      .add_option("display-layout",
                  description { "Set the window display layout." },
                  default_value<std::string> { "split" },
                  allowed<std::string> { {
                     "split", "toggle"
                  } })
      .add_option("display-mode",
                  description{ "Set the window display mode." },
                  default_value<std::string> { "windowed" },
                  allowed<std::string> { {
                     "windowed", "fullscreen"
                  } })
      .add_option("display-stretch",
                  description { "Enable display stretching, aspect ratio will not be maintained." })
      .add_option("sound",
                  description { "Enable sound output." })
      .add_option("dx12",
                  description { "Use DirectX 12 backend." });

   auto input_options = parser.add_option_group("Input Options")
      .add_option("vpad0",
                  description { "Select the input device for VPAD0." },
                  default_value<std::string> { "default_keyboard" });

   auto config_options = config::getExcmdGroups(parser);

   auto cmdPlay = parser.add_command("play")
      .add_option_group(frontend_options)
      .add_option_group(input_options)
      .add_argument("game directory", value<std::string> {});

   for (auto group : config_options) {
      cmdPlay.add_option_group(group);
   }

   return parser;
}

static std::string
getPathBasename(const std::string &path)
{
   auto pos = path.find_last_of("/\\");

   if (!pos) {
      return path;
   } else {
      return path.substr(pos + 1);
   }
}

static int
start(excmd::parser &parser,
      excmd::option_state &options)
{
   // Print version
   if (options.has("version")) {
      // TODO: print git hash
      std::cout << "Decaf Emulator version 0.0.1" << std::endl;
      std::exit(0);
   }

   // Print help
   if (options.empty() || options.has("help")) {
      if (options.has("help-command")) {
         std::cout << parser.format_help("decaf", options.get<std::string>("help-command")) << std::endl;
      } else {
         std::cout << parser.format_help("decaf") << std::endl;
      }

      std::exit(0);
   }

   if (!options.has("play")) {
      return 0;
   }

   auto gamePath = options.get<std::string>("game directory");

   // Load config file
   std::string configPath, configError;

   if (options.has("config")) {
      configPath = options.get<std::string>("config");
   } else {
      decaf::createConfigDirectory();
      configPath = decaf::makeConfigPath("config.toml");
   }

   // If config file does not exist, create a default one.
   if (!platform::fileExists(configPath)) {
      auto toml = cpptoml::make_table();
      config::saveToTOML(toml);
      config::saveFrontendToml(toml);
      std::ofstream out { configPath };
      out << (*toml);
   }

   try {
      auto toml = cpptoml::parse_file(configPath);
      config::loadFromTOML(toml);
      config::loadFrontendToml(toml);
   } catch (cpptoml::parse_exception ex) {
      configError = ex.what();
   }

   config::loadFromExcmd(options);

   // Now handle frontend config options
   if (options.has("vpad0")) {
       config::input::vpad0 = options.get<std::string>("vpad0");
   }

   if (options.has("display-mode")) {
       auto mode = options.get<std::string>("display-mode");

       if (mode.compare("windowed") == 0) {
           config::display::mode = config::display::Windowed;
       } else if (mode.compare("fullscreen") == 0) {
          config::display::mode = config::display::Fullscreen;
       } else {
           decaf_abort(fmt::format("Invalid display mode {}", mode));
       }
   }

   if (options.has("display-layout")) {
      auto layout = options.get<std::string>("display-layout");

      if (layout.compare("split") == 0) {
         config::display::layout = config::display::Split;
      } else if (layout.compare("toggle") == 0) {
         config::display::layout = config::display::Toggle;
      } else {
         decaf_abort(fmt::format("Invalid display layout {}", layout));
      }
   }

   if (options.has("display-stretch")) {
       config::display::stretch = true;
   }

   if (options.has("force-sync")) {
      config::display::force_sync = true;
   }

   if (options.has("backend")) {
      config::display::backend = options.get<std::string>("backend");
   }

   // Initialise libdecaf logger
   auto logFile = getPathBasename(gamePath);
   decaf::initialiseLogging(logFile);

   // Initialise frontend logger
   auto sinks = gLog->sinks();

   if (!decaf::config::log::to_stdout) {
      // Always do client log to stdout
      sinks.push_back(spdlog::sinks::stdout_sink_st::instance());
   }

   gCliLog = std::make_shared<spdlog::logger>("decaf-cli", begin(sinks), end(sinks));
   gCliLog->set_pattern("[%l] %v");
   gCliLog->info("Game path {}", gamePath);

   if (configError.empty()) {
      gCliLog->info("Loaded config from {}", configPath);
   } else {
      gCliLog->error("Failed to parse config {}: {}", configPath, configError);
   }

   DecafSDL sdl;

   if (!sdl.initCore()) {
      gCliLog->error("Failed to initialise SDL");
      return -1;
   }

   if (config::display::backend == "dx12") {
      if (!sdl.initDx12Graphics()) {
         gCliLog->error("Failed to initialise DX12 backend.");
         return -1;
      }
   } else if (config::display::backend == "vulkan") {
      if (!sdl.initVulkanGraphics()) {
         gCliLog->error("Failed to initialise Vulkan backend.");
         return -1;
      }
   } else if (config::display::backend == "opengl") {
      if (!sdl.initGlGraphics()) {
         gCliLog->error("Failed to initialise OpenGL backend.");
         return -1;
      }
   } else {
      gCliLog->error("Unknown display backend {}", config::display::backend);
      return -1;
   }

   if (options.has("sound") && !sdl.initSound()) {
      gCliLog->error("Failed to initialise SDL sound");
      return -1;
   }

   if (!sdl.run(gamePath)) {
      gCliLog->error("Failed to start game");
      return -1;
   }

   return 0;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
int WINAPI
wWinMain(_In_ HINSTANCE hInstance,
         _In_opt_ HINSTANCE hPrevInstance,
         _In_ LPWSTR lpCmdLine,
         _In_ int nShowCmd)
{
   auto parser = getCommandLineParser();
   excmd::option_state options;

   if (AttachConsole(ATTACH_PARENT_PROCESS)) {
      FILE *dumbFuck;
      freopen_s(&dumbFuck, "CONOUT$", "w", stdout);
      freopen_s(&dumbFuck, "CONOUT$", "w", stderr);
   }

   try {
      options = parser.parse(lpCmdLine);
   } catch (excmd::exception ex) {
      std::cout << "Error parsing options: " << ex.what() << std::endl;
      std::exit(-1);
   }

   return start(parser, options);
}
#else
int main(int argc, char **argv) {
   auto parser = getCommandLineParser();
   excmd::option_state options;

   try {
      options = parser.parse(argc, argv);
   } catch (excmd::exception ex) {
      std::cout << "Error parsing options: " << ex.what() << std::endl;
      std::exit(-1);
   }

   return start(parser, options);
}
#endif
