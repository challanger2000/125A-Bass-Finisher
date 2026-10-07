#pragma once

namespace HighGainGuitarFinisher::dsp {

class AutoLevelCompensator {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void processFrame(double referenceLeft,
                      double referenceRight,
                      double& processedLeft,
                      double& processedRight) noexcept;

    double currentGain() const noexcept {
        return gain_;
    }

    double currentGainDb() const noexcept;

private:
    static double timeCoefficient(double sampleRate,
                                  double milliseconds) noexcept;

    double sampleRate_ {44100.0};

    double inputEnergy_ {0.0};
    double outputEnergy_ {0.0};
    double activityEnergy_ {0.0};
    double gain_ {1.0};

    double energyCoefficient_ {0.0};
    double activityReleaseCoefficient_ {0.0};
    double gainUpCoefficient_ {0.0};
    double gainDownCoefficient_ {0.0};
    double bootstrapCoefficient_ {0.0};

    int bootstrapSamplesRemaining_ {0};
    int bootstrapLengthSamples_ {0};

    double minGain_ {1.0};
    double maxGain_ {1.0};
    double gateEnergy_ {0.0};
};

} // namespace HighGainGuitarFinisher::dsp
