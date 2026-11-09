#ifndef DENSE_FLOW_DIS_H
#define DENSE_FLOW_DIS_H

#include "OpenCVCommon.h"

class DenseFlowDis {
public:
    enum class Preset {
        UltraFast,
        Fast,
        Medium
    };

    explicit DenseFlowDis(Preset preset = Preset::Fast)
        : m_preset(preset) {
        Recreate();
    }

    [[nodiscard]] cv::Mat Compute(
        const cv::Mat& prevGray,
        const cv::Mat& gray) const {
        cv::Mat flow;
        m_dis->calc(prevGray, gray, flow);
        return flow;
    }

    void SetPreset(Preset preset) {
        if (m_preset == preset) {
            return;
        }
        m_preset = preset;
        Recreate();
    }

    [[nodiscard]] Preset GetPreset() const {
        return m_preset;
    }

    [[nodiscard]] const char* PresetName() const {
        switch (m_preset) {
            case Preset::UltraFast: return "UltraFast";
            case Preset::Fast:      return "Fast";
            case Preset::Medium:    return "Medium";
        }
        return "Unknown";
    }

    void CyclePreset() {
        switch (m_preset) {
            case Preset::UltraFast: SetPreset(Preset::Fast);      break;
            case Preset::Fast:      SetPreset(Preset::Medium);    break;
            case Preset::Medium:    SetPreset(Preset::UltraFast); break;
        }
    }

private:
    void Recreate() {
        int presetFlag = cv::DISOpticalFlow::PRESET_FAST;
        if (m_preset == Preset::UltraFast) {
            presetFlag = cv::DISOpticalFlow::PRESET_ULTRAFAST;
        } else if (m_preset == Preset::Medium) {
            presetFlag = cv::DISOpticalFlow::PRESET_MEDIUM;
        }
        m_dis = cv::DISOpticalFlow::create(presetFlag);
    }

    Preset m_preset;
    cv::Ptr<cv::DISOpticalFlow> m_dis;
};

#endif