#include "media/ffmpeg/FfmpegSupport.h"

extern "C" {
#include <libavutil/error.h>
}

namespace Media::Ffmpeg {

QString errorString(int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(error, buffer, sizeof(buffer)) < 0)
        return QStringLiteral("FFmpeg error %1").arg(error);
    return QString::fromUtf8(buffer);
}

} // namespace Media::Ffmpeg
