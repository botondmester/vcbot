#include "MyBot.h"
#include <dpp/dpp.h>
#include "vcbparser.h"
#include "lodepng.h"

std::string BOT_TOKEN; // place in a file called "token.txt", next to this file

dpp::message generateImage(const std::string &bp) {
    const auto start = std::chrono::steady_clock::now();

    VcbCircuit circ = VcbParser::parseBP(bp);

    const auto mid = std::chrono::steady_clock::now();

    std::vector<unsigned char> png;

    lodepng::encode(png, circ.blocks[circ.logic].buffer, circ.width, circ.height);

    dpp::message msg("");
    std::string cont(png.begin(), png.end());
    msg.add_file("circ.png", cont);

    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> time = end - start;
    const std::chrono::duration<double, std::milli> parse = mid - start;
    const std::chrono::duration<double, std::milli> encode = end - mid;
    std::cout << "[INFO]: rendering BP took " << time.count() << "ms!\n";
    std::cout << "\t\tparse:\t" << parse.count() << "ms\n";
    std::cout << "\t\tencode:\t" << encode.count() << "ms\n";
    return msg;
}

int main()
{
#ifdef TEST_PARSER

    VcbParser::test();

#else
    std::ifstream file;
    std::stringstream buffer;
    file.open("token.txt", std::ios::in | std::ios::binary);
    if (file.is_open()) {
        buffer << file.rdbuf();
    }
    else return 0;
    file.close();
    BOT_TOKEN = buffer.str();
    /* Create bot cluster */
    dpp::cluster bot(BOT_TOKEN);

    /* Output simple log messages to stdout */
    bot.on_log(dpp::utility::cout_logger());

    /* Handle slash command */
    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.command.get_creation_time();
            event.reply("Pong!");
        }
        if (event.command.get_command_name() == "image") {
            auto subcommand = event.command.get_command_interaction().options[0];
            if (subcommand.options.empty()) {
                event.reply("no arguments specified!");
            }
            if (subcommand.name == "text") {
                std::string bp = subcommand.get_value<std::string>(0);
                try {
                    event.reply(generateImage(bp));
                }
                catch (std::invalid_argument& e) {
                    std::cout << "[ERROR]: " << e.what() << '\n';
                    event.reply("Failed to generate image!");
                }
            }
            else if (subcommand.name == "file") {
                dpp::snowflake file_id = subcommand.get_value<dpp::snowflake>(0);
                const dpp::attachment& att = event.command.get_resolved_attachment(file_id);
                bot.request(att.url, dpp::m_get, [event](const dpp::http_request_completion_t& cc) {
                    if (cc.error) {
                        event.reply("Failed to download file");
                    }
                    std::string bp = cc.body;
                    try {
                        event.reply(generateImage(bp));
                    }
                    catch (std::invalid_argument& e) {
                        std::cout << "[ERROR]: " << e.what() << '\n';
                        event.reply("Failed to generate image!");
                    }
                });
            }
        }
        });

    /* Register slash command here in on_ready */
    bot.on_ready([&bot](const dpp::ready_t& event) {
        /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
            dpp::slashcommand imageCommand("image", "E", bot.me.id);
            imageCommand.add_option(dpp::command_option(dpp::co_sub_command, "text", "generate from text")
                .add_option(dpp::command_option(dpp::co_string, "bp", "a", true)));
            imageCommand.add_option(dpp::command_option(dpp::co_sub_command, "file", "generate from file")
                .add_option(dpp::command_option(dpp::co_attachment, "bp", "a", true)));

            bot.global_command_create(imageCommand);
        }
        });

    /* Start the bot */
    bot.start(dpp::st_wait);

#endif // TEST_PARSER

    return 0;
}
