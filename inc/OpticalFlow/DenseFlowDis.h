#ifndef INC_OPTICALFLOW_DENSEFLOWDIS_H
#define INC_OPTICALFLOW_DENSEFLOWDIS_H

#include "OpenCVCommon.h"

class DenseFlowDis {
public:
    enum class Preset { UltraFast, Fast, Medium };

    explicit DenseFlowDis(Preset preset = Preset::Fast)
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
        case Preset::UltraFast:
            return "UltraFast";
        case Preset::Fast:
            return "Fast";
        case Preset::Medium:
            return "Medium";
        }
        return "Unknown";
    }

    void CyclePreset()
    {
        switch (preset_) {
        case Preset::UltraFast:
            SetPreset(Preset::Fast);
            break;
        case Preset::Fast:
            SetPreset(Preset::Medium);
            break;
        case Preset::Medium:
            SetPreset(Preset::UltraFast);
            break;
        }
    }

private:
    void Recreate()
    {
        int presetFlag = cv::DISOpticalFlow::PRESET_FAST;
        if (preset_ == Preset::UltraFast) {
            presetFlag = cv::DISOpticalFlow::PRESET_ULTRAFAST;
        } else if (preset_ == Preset::Medium) {
            presetFlag = cv::DISOpticalFlow::PRESET_MEDIUM;
        }
        dis_ = cv::DISOpticalFlow::create(presetFlag);
    }

    Preset preset_;
    cv::Ptr<cv::DISOpticalFlow> dis_;
};

#endif