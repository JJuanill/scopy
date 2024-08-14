#ifndef ADMTCONTROLLER_H
#define ADMTCONTROLLER_H

#include "scopy-admt_export.h"

#include <iio.h>
#include <bitset>

#include <QObject>
#include <QString>

#include <iioutil/connectionprovider.h>

#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <numeric>
#include <complex>
#include <iterator>

using namespace std;

namespace scopy::admt {    
class SCOPY_ADMT_EXPORT ADMTController : public QObject
{
    Q_OBJECT
public:
    ADMTController(QString uri, QObject *parent = nullptr);
    ~ADMTController();

    int HAR_MAG_1, HAR_MAG_2, HAR_MAG_3, HAR_MAG_8 ,HAR_PHASE_1 ,HAR_PHASE_2 ,HAR_PHASE_3 ,HAR_PHASE_8;

    vector<double> angle_errors_fft;
    vector<double> angle_errors_fft_phase;

    enum Channel
    {
        ROTATION,
        ANGLE,
        COUNT,
        TEMPERATURE,
        CHANNEL_COUNT
    };

    enum Device
    {
        ADMT4000,
        TMC5240,
        DEVICE_COUNT
    };

    enum MotorAttribute
    {
        AMAX,
        ROTATE_VMAX,
        DMAX,
        DISABLE,
        TARGET_POS,
        CURRENT_POS,
        RAMP_MODE,
        MOTOR_ATTR_COUNT
    };

    const char* ChannelIds[CHANNEL_COUNT] = {"rot", "angl", "count", "temp"};
    const char* DeviceIds[DEVICE_COUNT] = {"admt4000", "tmc5240"};
    const char* MotorAttributes[MOTOR_ATTR_COUNT] = {"amax", "rotate_vmax", "dmax",
                                                     "disable", "target_pos", "current_pos",
                                                     "ramp_mode"};

    const char* getChannelId(Channel channel);
    const char* getDeviceId(Device device);
    const char* getMotorAttribute(MotorAttribute attribute);

    void connectADMT();
    void disconnectADMT();
    int getChannelIndex(const char *deviceName, const char *channelName);
    double getChannelValue(const char *deviceName, const char *channelName, int bufferSize = 1);
    int getDeviceAttributeValue(const char *deviceName, const char *attributeName, double *returnValue);
    int setDeviceAttributeValue(const char *deviceName, const char *attributeName, double writeValue);
    QString calibrate(vector<double> PANG, int cycles = 11, int samplesPerCycle = 256);
    int writeDeviceRegistry(const char *deviceName, uint32_t address, double value);
private:
    iio_context *m_iioCtx;
    iio_buffer *m_iioBuffer;
    Connection *m_conn;
    QString uri;

    unsigned int bitReverse(unsigned int x, int log2n);
    template<class Iter_T>
    void fft(Iter_T a, Iter_T b, int log2n);
    int linear_fit(vector<double> x, vector<double> y, double* slope, double* intercept);
    void reverseArray(vector<double>& PANG, bool CCW);
    void randomizeArray(vector<double>& PANG, bool circshiftData);
    void calculateAngleErrors(const vector<double>& PANG, vector<double>& angle_errors, double& max_err);
    void performFFT(const vector<double>& angle_errors, int samplesPerCycle, int cycles, vector<double>& angle_errors_fft, vector<double>& angle_errors_fft_phase);
    void extractHarmonics(const vector<double>& angle_errors_fft, int cycles, double& H1Mag, double& H2Mag, double& H3Mag, double& H8Mag);
    void extractPhases(const vector<double>& angle_errors_fft_phase, int cycles, double& H1Phase, double& H2Phase, double& H3Phase, double& H8Phase);
    void applyCorrections(const vector<double>& PANG, double H1Mag, double H2Mag, double H3Mag, double H8Mag, double H1Phase, double H2Phase, double H3Phase, double H8Phase, vector<double>& HXcorrection);
    void deriveRegisterValues(double H1Mag, double H2Mag, double H3Mag, double H8Mag, double H1Phase, double H2Phase, double H3Phase, double H8Phase, int& HAR_MAG_1, int& HAR_MAG_2, int& HAR_MAG_3, int& HAR_MAG_8, int& HAR_PHASE_1, int& HAR_PHASE_2, int& HAR_PHASE_3, int& HAR_PHASE_8);
    void calculateErrorResults(const vector<double>& angle_errors_fft, vector<double>& ErrorMagnitudeResult, vector<double>& ErrorPhaseResult);
};
} // namespace scopy::admt

#endif // ADMTCONTROLLER_H