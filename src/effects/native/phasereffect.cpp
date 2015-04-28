#include <QDebug>
#include "util/math.h"
#include "effects/native/phasereffect.h"
#include <stdlib.h>

using namespace std;

// static
QString PhaserEffect::getId() {
    return "org.mixxx.effects.phaser";
}

// static
EffectManifest PhaserEffect::getManifest() {
    EffectManifest manifest;
    manifest.setId(getId());
    manifest.setName(QObject::tr("Phaser"));
    manifest.setAuthor("The Mixxx Team");
    manifest.setVersion("1.0");
    manifest.setDescription(QObject::tr(
                "A more complex sound effect obtained by mixing the input signal" 
                "with a copy passed through a series of all-pass filters."));
    
    EffectManifestParameter* stages = manifest.addParameter();
    stages->setId("stages");
    stages->setName(QObject::tr("Stages"));
    stages->setDescription("Sets number of stages.");
    stages->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    stages->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    stages->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    stages->setDefault(1.0);
    stages->setMinimum(1.0);
    stages->setMaximum(12.0);

    EffectManifestParameter* frequency = manifest.addParameter();
    frequency->setId("frequency");
    frequency->setName(QObject::tr("Frequency"));
    frequency->setDescription("Controls frequency.");
    frequency->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    frequency->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    frequency->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    frequency->setDefault(0.5);
    frequency->setMinimum(0.0);
    frequency->setMaximum(5.0);

    EffectManifestParameter* depth = manifest.addParameter();
    depth->setId("depth");
    depth->setName(QObject::tr("Depth"));
    depth->setDescription("Controls depth.");
    depth->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    depth->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    depth->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    depth->setDefault(1.0);
    depth->setMinimum(0.0);
    depth->setMaximum(1.0);

    EffectManifestParameter* fb = manifest.addParameter();
    fb->setId("feedback");
    fb->setName(QObject::tr("Feedback"));
    fb->setDescription("Feedback");
    fb->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    fb->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    fb->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    fb->setDefault(0.5);
    fb->setMinimum(0.0);
    fb->setMaximum(1.0);

    EffectManifestParameter* sweep = manifest.addParameter();
    sweep->setId("sweep_width");
    sweep->setName(QObject::tr("Sweep"));
    sweep->setDescription("Sets sweep width.");
    sweep->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    sweep->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    sweep->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    sweep->setDefault(0.5);
    sweep->setMinimum(0.0);
    sweep->setMaximum(1.0);

    EffectManifestParameter* stereo = manifest.addParameter();
    stereo->setId("stereo");
    stereo->setName(QObject::tr("Stereo"));
    stereo->setDescription(QObject::tr("Enables stereo"));
    stereo->setControlHint(EffectManifestParameter::CONTROL_TOGGLE_STEPPING);
    stereo->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    stereo->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    stereo->setDefault(0);
    stereo->setMinimum(0);
    stereo->setMaximum(1);
    return manifest;
}

PhaserEffect::PhaserEffect(EngineEffect* pEffect,
                           const EffectManifest& manifest) 
        : m_pStagesParameter(pEffect->getParameterById("stages")),
          m_pFrequencyParameter(pEffect->getParameterById("frequency")),
          m_pDepthParameter(pEffect->getParameterById("depth")),
          m_pFeedbackParameter(pEffect->getParameterById("feedback")),
          m_pSweepWidthParameter(pEffect->getParameterById("sweep_width")),
          m_pStereoParameter(pEffect->getParameterById("stereo")) {
    Q_UNUSED(manifest);
}

PhaserEffect::~PhaserEffect() {
    //qDebug() << debugString() << "destroyed";
}

void PhaserEffect::processChannel(const ChannelHandle& handle,
                                  PhaserGroupState* pState,
                                  const CSAMPLE* pInput, CSAMPLE* pOutput,
                                  const unsigned int numSamples,
                                  const unsigned int sampleRate,
                                  const EffectProcessor::EnableState enableState,
                                  const GroupFeatureState& groupFeatures) {

    Q_UNUSED(handle);
    Q_UNUSED(enableState);
    Q_UNUSED(groupFeatures);
    Q_UNUSED(sampleRate);

    CSAMPLE frequency = m_pFrequencyParameter->value();
    CSAMPLE depth = m_pDepthParameter->value();
    CSAMPLE feedback = m_pFeedbackParameter->value();
    CSAMPLE sweepWidth = m_pSweepWidthParameter->value();
    int stages = 2 * m_pStagesParameter->value();

    CSAMPLE* oldInLeft = pState->oldInLeft;
    CSAMPLE* oldOutLeft = pState->oldOutLeft;
    CSAMPLE* oldInRight = pState->oldInRight;
    CSAMPLE* oldOutRight = pState->oldOutRight;

    CSAMPLE filterCoefLeft = 0;
    CSAMPLE filterCoefRight = 0;

    CSAMPLE left = 0, right = 0;
    CSAMPLE leftPhase, rightPhase;
    CSAMPLE freqSkip = frequency * 2.0 * M_PI / sampleRate;

    int stereoCheck = m_pStereoParameter->value();
    int counter = 0;
    int updateCoef = 16;

    const int kChannels = 2;
    for (unsigned int i = 0; i < numSamples; i += kChannels) {

        pState->time++;
        left = pInput[i] + left * feedback; 
        right = pInput[i + 1] + right * feedback;

        leftPhase = fmodf(freqSkip * pState->time, 2.0 * M_PI);
        rightPhase = fmodf(freqSkip * pState->time + M_PI * stereoCheck, 2.0 * M_PI);

        if ((counter++) % updateCoef == 0) {
                CSAMPLE delayLeft = 0.5 + 0.5 * sin(leftPhase);
                CSAMPLE delayRight = 0.5 + 0.5 * sin(rightPhase);
                
                delayLeft = min((double)(sweepWidth * delayLeft), 0.99 * M_PI);
                delayRight =  min((double)(sweepWidth * delayRight), 0.99 * M_PI);

                delayLeft = tan(delayLeft / 2);
                delayRight = tan(delayRight / 2);

                filterCoefLeft = (1.0 - delayLeft) / (1.0 + delayLeft);
                filterCoefRight = (1.0 - delayRight) / (1.0 + delayRight);
        }

        for (int j = 0; j < stages; j++) {
            oldOutLeft[j] = (filterCoefLeft * left) +
                (filterCoefLeft * oldOutLeft[j]) - oldInLeft[j];
            oldInLeft[j] = left;
            left = oldOutLeft[j];

            oldOutRight[j] = (filterCoefRight * right) +
                (filterCoefRight * oldOutRight[j]) - oldInRight[j];
            oldInRight[j] = right;
            right = oldOutRight[j];
        }
        pOutput[i] = pInput[i] * (1.0 - 0.5 * depth) + left * depth * 0.5;
        pOutput[i + 1] = pInput[i + 1] * (1.0 - 0.5 * depth) + right * depth * 0.5;
    }
}
