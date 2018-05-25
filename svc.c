#include "svc.h"
#include "svc_twitch.h"

struct svc svcs[SVCS_AMNT] = {
[SVCS_TWITCH] = {
        .name           = "Twitch.tv",
        .shrtname       = "TTV",
        .cfg_suf        = "twitch",
        .api_key        = "onsyu6idu0o41dl4ixkofx6pqq7ghn",
        .local_update   = ttv_local_update,
        .perform        = ttv_perform,
        .stream_play    = ttv_stream_play,
        .stream_quality = ttv_stream_quality,
        .cleanup        = ttv_cleanup,
        .up_count       = ttv_up_count,
        .total_count    = ttv_total_count,
        .chans          = ttv_chans,
        .status         = ttv_status,
        .game           = ttv_game
}
};

const char *ext_tool[EXT_TOOL_AMNT] = {
        "youtube-dl",
        "streamlink"
};
