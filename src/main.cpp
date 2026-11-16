#include "ts_parser_gui.h"

bool app_running = true;

int main(int argc, char **argv)
{
    std::shared_ptr<TsParserGui> h = std::make_shared<TsParserGui>();
    if (!h->Init())
    {
        return 1;
    }

    h->Do();

    h = nullptr;
    return 0;
}
