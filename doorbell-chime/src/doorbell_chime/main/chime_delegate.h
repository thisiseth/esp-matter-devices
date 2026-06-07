#pragma once

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/chime-server/chime-server.h>

#include "esp_log.h"

namespace chip {
namespace app {
namespace Clusters {
namespace Chime {

class ChimeDelegateImpl : public ChimeDelegate {
public:
    ChimeDelegateImpl() = default;
    ~ChimeDelegateImpl() override = default;

    using PlayChimeSoundCallback = void (*)();
    PlayChimeSoundCallback SoundCallback = nullptr;

    CHIP_ERROR GetChimeSoundByIndex(uint8_t chimeIndex, uint8_t &chimeID, MutableCharSpan &name) override {
        ESP_LOGI(TAG, "GetChimeSoundByIndex with id %d", chimeIndex);

        if (chimeIndex != 0)
            return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;

        chimeID = 0;
        return CopyCharSpanToMutableCharSpan(CharSpan::fromCharString("bong"), name);
    }

    CHIP_ERROR GetChimeIDByIndex(uint8_t chimeIndex, uint8_t &chimeID) override {
        ESP_LOGI(TAG, "GetChimeIDByIndex with id %d", chimeIndex);

        if (chimeIndex != 0)
            return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;

        chimeID = 0;

        return CHIP_NO_ERROR;
    }

    Protocols::InteractionModel::Status PlayChimeSound() override {
        ESP_LOGI(TAG, "PlayChimeSound call");

        if (SoundCallback)
            SoundCallback();
        else
            ESP_LOGE(TAG, "SoundCallback is null");

        return Protocols::InteractionModel::Status::Success;
    }

private:
    const char *TAG = "chime";
};

} // namespace Chime
} // namespace Clusters
} // namespace app
} // namespace chip