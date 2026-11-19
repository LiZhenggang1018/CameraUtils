#ifndef INC_OPTICALFLOW_DENSEFLOWDIS_H
#define INC_OPTICALFLOW_DENSEFLOWDIS_H

#include "OpenCVCommon.h"

class DenseFlowDis {
public:
    enum class Preset { ULTRA_FAST, FAST, MEDIUM };

    explicit DenseFlowDis(Preset preset = Preset::FAST)
        : preset_(preset)
    {
        Recreate();
    }

    [[nodiscard]] cv::Mat Compute(const cv::Mat& prevGray, const cv::Mat& gray) const
    {
        cv::Mat flow;
        dis_->calc(prevGray, gray, flow);
        return flow;
    }

    void SetPreset(Preset preset)
    {
        if (preset_ == preset) {
            return;
        }
        preset_ = preset;
        Recreate();
    }

    [[nodiscard]] Preset GetPreset() const { return preset_; }

    [[nodiscard]] const char* PresetName() const
    {
        switch (preset_) {
        case Preset::ULTRA_FAST:
            return "UltraFast";
        case Preset::FAST:
            return "Fast";
        case Preset::MEDIUM:
            return "Medium";
        }
        return "Unknown";
    }

    void CyclePreset()
    {
        switch (preset_) {
        case Preset::ULTRA_FAST:
            SetPreset(Preset::FAST);
            break;
        case Preset::FAST:
            SetPreset(Preset::MEDIUM);
            break;
        case Preset::MEDIUM:
            SetPreset(Preset::ULTRA_FAST);
            break;
        }
    }

private:
    void Recreate()
    {
        int presetFlag = cv::DISOpticalFlow::PRESET_FAST;
        if (preset_ == Preset::ULTRA_FAST) {
            presetFlag = cv::DISOpticalFlow::PRESET_ULTRAFAST;
        } else if (preset_ == Preset::MEDIUM) {
            presetFlag = cv::DISOpticalFlow::PRESET_MEDIUM;
        }
        dis_ = cv::DISOpticalFlow::create(presetFlag);
    }

    Preset preset_;
    cv::Ptr<cv::DISOpticalFlow> dis_;
};

#endif