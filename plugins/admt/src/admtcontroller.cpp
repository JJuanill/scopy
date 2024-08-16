#include "admtcontroller.h"

#include <iioutil/connectionprovider.h>

#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <list>

#include <vector>
#include <cstdlib>
#include <cmath>
#include <numeric>
#include <complex>
#include <iterator>

static const size_t maxAttrSize = 512;

using namespace scopy::admt;
using namespace std;

ADMTController::ADMTController(QString uri, QObject *parent)
    :QObject(parent)
    , uri(uri)
{

}

ADMTController::~ADMTController() {}

void ADMTController::connectADMT()
{
	m_conn = ConnectionProvider::open(uri);
    m_iioCtx = m_conn->context();
}

void ADMTController::disconnectADMT()
{
	if(!m_conn || !m_iioCtx){
		return;
	}

	ConnectionProvider::close(uri);
	m_conn = nullptr;
	m_iioCtx = nullptr;

}

const char* ADMTController::getChannelId(Channel channel)
{
    return (channel >= 0 && channel < CHANNEL_COUNT) ? ChannelIds[channel] : "Unknown";
}

const char* ADMTController::getDeviceId(Device device)
{
    return (device >= 0 && device < DEVICE_COUNT) ? DeviceIds[device] : "Unknown";
}

const char* ADMTController::getMotorAttribute(MotorAttribute attribute)
{
    return (attribute >= 0 && attribute < MOTOR_ATTR_COUNT) ? MotorAttributes[attribute] : "Unknown";
}

int ADMTController::getChannelIndex(const char *deviceName, const char *channelName)
{
    iio_device *admtDevice = iio_context_find_device(m_iioCtx, deviceName);
    if (!admtDevice) return -1;

    int channelCount = iio_device_get_channels_count(admtDevice);
    for (int i = 0; i < channelCount; ++i) {
        iio_channel *channel = iio_device_get_channel(admtDevice, i);
        const char *deviceChannel = iio_channel_get_id(channel);

        if (deviceChannel && strcmp(deviceChannel, channelName) == 0) {
            return iio_channel_get_index(channel);
        }
    }
    return -1;
}

double ADMTController::getChannelValue(const char *deviceName, const char *channelName, int bufferSize)
{
    iio_device *admtDevice = iio_context_find_device(m_iioCtx, deviceName);
    if (!admtDevice) return 0.0;

    int channelCount = iio_device_get_channels_count(admtDevice);
    iio_channel *channel = nullptr;

    for (int i = 0; i < channelCount; i++) {
        iio_channel *tempChannel = iio_device_get_channel(admtDevice, i);
        const char *deviceChannel = iio_channel_get_id(tempChannel);

        if (deviceChannel && strcmp(deviceChannel, channelName) == 0) {
            channel = tempChannel;
            break;
        }
    }

    if (!channel) return 0.0;

    iio_channel_enable(channel);

    double scale = 1.0;
    int offsetAttrVal = 0;

    if (iio_channel_attr_read_double(channel, "scale", &scale) != 0) return 0.0;

    char offsetBuffer[maxAttrSize] = "";
    if (iio_channel_attr_read(channel, "offset", offsetBuffer, maxAttrSize) > 0) {
        offsetAttrVal = std::atoi(offsetBuffer);
    }

    iio_buffer *iioBuffer = iio_device_create_buffer(admtDevice, bufferSize, false);
    if (!iioBuffer) return 0.0;

    ssize_t numBytesRead = iio_buffer_refill(iioBuffer);
    if (numBytesRead < 1) {
        iio_buffer_destroy(iioBuffer);
        return 0.0;
    }

    const struct iio_data_format *format = iio_channel_get_data_format(channel);
    size_t sampleSize = format->length / 8 * format->repeat;
    if (sampleSize == 0) {
        iio_buffer_destroy(iioBuffer);
        return 0.0;
    }

    void *buffer = malloc(sampleSize * bufferSize);
    if (!buffer) {
        iio_buffer_destroy(iioBuffer);
        return 0.0;
    }

    double value = 0.0;
    size_t bytes = iio_channel_read(channel, iioBuffer, buffer, sampleSize * bufferSize);

    for (size_t sample = 0; sample < bytes / sampleSize; ++sample) {
        for (int j = 0; j < format->repeat; ++j) {
            if (format->length / 8 == sizeof(int16_t)) {
                int16_t rawValue = ((int16_t*)buffer)[sample + j];
                value = (rawValue - static_cast<int16_t>(offsetAttrVal)) * scale;
            }
        }
    }

    free(buffer);
    iio_buffer_destroy(iioBuffer);
    return value;
}


/** @brief Get the attribute value of a device
 * @param deviceName A pointer to the device name
 * @param attributeName A NULL-terminated string corresponding to the name of the
 * attribute
 * @param returnValue A pointer to a double variable where the value should be stored
 * @return On success, 0 is returned.
 * @return On error, -1 is returned. */
int ADMTController::getDeviceAttributeValue(const char *deviceName, const char *attributeName, double *returnValue)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
	iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    const char* hasAttr = iio_device_find_attr(iioDevice, attributeName);
    if(hasAttr == NULL) { return result; }
    result = iio_device_attr_read_double(iioDevice, attributeName, returnValue);

    return result;
}

/** @brief Set the attribute value of a device
 * @param deviceName A pointer to the device name
 * @param attributeName A NULL-terminated string corresponding to the name of the
 * attribute
 * @param writeValue A double variable of the value to be set
 * @return On success, 0 is returned.
 * @return On error, -1 is returned. */
int ADMTController::setDeviceAttributeValue(const char *deviceName, const char *attributeName, double writeValue)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
	iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    const char* hasAttr = iio_device_find_attr(iioDevice, attributeName);
    if(hasAttr == NULL) { return result; }
    result = iio_device_attr_write_double(iioDevice, attributeName, writeValue);

    return result;
}

int ADMTController::writeDeviceRegistry(const char *deviceName, uint32_t address, double value)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
    iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    result = iio_device_reg_write(iioDevice, address, static_cast<uint32_t>(value));

    return result;
}

// Main calibration function
QString ADMTController::calibrate(vector<double> PANG, int cycles, int samplesPerCycle) {
    int CCW = 0, circshiftData = 0;
    QString result = "";

    // Reverse the array based on CCW flag
    reverseArray(PANG, CCW);

    // Randomize the array based on circshiftData flag
    randomizeArray(PANG, circshiftData);

    // Calculate angle errors and find the maximum error
    double max_err = 0;
    vector<double> angle_errors(PANG.size());
    calculateAngleErrors(PANG, angle_errors, max_err);

    // Perform FFT on angle errors
    angle_errors_fft = vector<double>(PANG.size() / 2);
    angle_errors_fft_phase = vector<double>(PANG.size() / 2);
    performFFT(angle_errors, samplesPerCycle, cycles, angle_errors_fft, angle_errors_fft_phase);

    // Extract harmonic magnitudes
    double H1Mag, H2Mag, H3Mag, H8Mag;
    extractHarmonics(angle_errors_fft, cycles, H1Mag, H2Mag, H3Mag, H8Mag);

    // Extract harmonic phases
    double H1Phase, H2Phase, H3Phase, H8Phase;
    extractPhases(angle_errors_fft_phase, cycles, H1Phase, H2Phase, H3Phase, H8Phase);

    // Apply corrections to the data
    vector<double> HXcorrection(PANG.size());
    applyCorrections(PANG, H1Mag, H2Mag, H3Mag, H8Mag, H1Phase, H2Phase, H3Phase, H8Phase, HXcorrection);

    // Derive register values for harmonics
    int HAR_MAG_1, HAR_MAG_2, HAR_MAG_3, HAR_MAG_8;
    int HAR_PHASE_1, HAR_PHASE_2, HAR_PHASE_3, HAR_PHASE_8;
    deriveRegisterValues(H1Mag, H2Mag, H3Mag, H8Mag, H1Phase, H2Phase, H3Phase, H8Phase, HAR_MAG_1, HAR_MAG_2, HAR_MAG_3, HAR_MAG_8, HAR_PHASE_1, HAR_PHASE_2, HAR_PHASE_3, HAR_PHASE_8);

    // Calculate error magnitude and phase results
    vector<double> ErrorMagnitudeResult(angle_errors_fft.size());
    vector<double> ErrorPhaseResult(angle_errors_fft.size());
    calculateErrorResults(angle_errors_fft, ErrorMagnitudeResult, ErrorPhaseResult);

    // Append the register values to the result string
    result.append("HMAG1: " + QString::number(HAR_MAG_1) + "\n");
    result.append("HMAG2: " + QString::number(HAR_MAG_2) + "\n");
    result.append("HMAG3: " + QString::number(HAR_MAG_3) + "\n");
    result.append("HMAG8: " + QString::number(HAR_MAG_8) + "\n");
    result.append("HPHASE1: " + QString::number(HAR_PHASE_1) + "\n");
    result.append("HPHASE2: " + QString::number(HAR_PHASE_2) + "\n");
    result.append("HPHASE3: " + QString::number(HAR_PHASE_3) + "\n");
    result.append("HPHASE8: " + QString::number(HAR_PHASE_8) + "\n");

    return result;
}

/* bit reversal from online example */
unsigned int ADMTController::bitReverse(unsigned int x, int log2n) {
    int n = 0;
    int mask = 0x1;
    for (int i = 0; i < log2n; i++) {
        n <<= 1;
        n |= (x & 1);
        x >>= 1;
    }
    return n;
}

template<class Iter_T>
/* fft from example codes online */
void ADMTController::fft(Iter_T a, Iter_T b, int log2n)
{
    typedef typename iterator_traits<Iter_T>::value_type complex;
    const complex J(0, 1);
    int n = 1 << log2n;
    for (unsigned int i = 0; i < n; ++i) {
        b[bitReverse(i, log2n)] = a[i];
    }
    for (int s = 1; s <= log2n; ++s) {
        int m = 1 << s;
        int m2 = m >> 1;
        complex w(1, 0);
        complex wm = exp(-J * (M_PI / m2));
        for (int j = 0; j < m2; ++j) {
            for (int k = j; k < n; k += m) {
                complex t = w * b[k + m2];
                complex u = b[k];
                b[k] = u + t;
                b[k + m2] = u - t;
            }
            w *= wm;
        }
    }
}

/* For linear fitting (hard-coded based on examples and formula for polynomial fitting) */
int ADMTController::linear_fit(vector<double> x, vector<double> y, double* slope, double* intercept)
{
    /* x, y, x^2, y^2, xy, xy^2 */
    double sum_x = 0, sum_y = 0, sum_x2 = 0, sum_y2 = 0, sum_xy = 0;
    int i;

    if (x.size() != y.size())
        return -22;

    for (i = 0; i < x.size(); i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_x2 += (x[i] * x[i]);
        sum_y2 += (y[i] * y[i]);
        sum_xy += (x[i] * y[i]);
    }

    *slope = (x.size() * sum_xy - sum_x * sum_y) / (x.size() * sum_x2 - sum_x * sum_x);

    *intercept = (sum_y * sum_x2 - sum_x * sum_xy) / (x.size() * sum_x2 - sum_x * sum_x);

    return 0;
}

/* Calculate angle error based on MATLAB and C# implementation */
void ADMTController::calculateAngleErrors(const vector<double>& PANG, vector<double>& angle_errors, double& max_err)
{
    vector<double> angle_meas_rad(PANG.size()); // radian converted input
    vector<double> angle_meas_rad_unwrap(PANG.size()); // unwrapped radian input
    vector<double> angle_fit(PANG.size()); // array for polynomial fitted data 
    vector<double> x_data(PANG.size());
    double coeff_a, coeff_b; // coefficients generated by polynomial fitting

    // convert to radian
    for (int i = 0; i < angle_meas_rad.size(); i++)
        angle_meas_rad[i] = PANG[i] * M_PI / 180.0;

    // unwrap angle (extracted from decompiled Angle GSF Unit
    double num = 0.0;
    angle_meas_rad_unwrap[0] = angle_meas_rad[0];
    for (int i = 1; i < angle_meas_rad.size(); i++)
    {
        double num2 = abs(angle_meas_rad[i] + num - angle_meas_rad_unwrap[i - 1]);
        double num3 = abs(angle_meas_rad[i] + num - angle_meas_rad_unwrap[i - 1] + M_PI * 2.0);
        double num4 = abs(angle_meas_rad[i] + num - angle_meas_rad_unwrap[i - 1] - M_PI * 2.0);
        if (num3 < num2 && num3 < num4)
            num += M_PI * 2.0;

        else if (num4 < num2 && num4 < num3)
            num -= M_PI * 2.0;

        angle_meas_rad_unwrap[i] = angle_meas_rad[i] + num;
    }

    // set initial point to zero
    double offset = angle_meas_rad_unwrap[0];
    for (int i = 0; i < angle_meas_rad_unwrap.size(); ++i)
        angle_meas_rad_unwrap[i] -= offset;

    /* Generate xdata for polynomial fitting */
    iota(x_data.begin(), x_data.end(), 1);

    // linear angle fitting (generated coefficients not same with matlab and python)
    // expecting 0.26 -0.26
    // getting ~0.27 ~-0.27 as of 4/2/2024
    /* input args: x, y, *slope, *intercept */
    linear_fit(x_data, angle_meas_rad_unwrap, &coeff_a, &coeff_b);

    //cout << coeff_a << " " << coeff_b << "\n";

    // generate data using coefficients from polynomial fitting
    for (int i = 0; i < angle_fit.size(); i++) {
        angle_fit[i] = coeff_a * x_data[i];
        //cout << "angle_fit " << angle_fit[i] << "\n";
    }

    // get angle error using pass by ref angle_errors
    for (int i = 0; i < angle_errors.size(); i++) {
        angle_errors[i] = angle_meas_rad_unwrap[i] - angle_fit[i];
        //cout << "angle_err_ret " << angle_errors[i] << "\n";
    }

    // Find the offset for error and subtract (using angle_errors)
    auto minmax = minmax_element(angle_errors.begin(), angle_errors.end());
    double angle_err_offset = (*minmax.first + *minmax.second) / 2;

    for (int i = 0; i < angle_errors.size(); i++)
        angle_errors[i] -= angle_err_offset;

    // Convert back to degrees (angle_errors)
    for (int i = 0; i < PANG.size(); i++)
        angle_errors[i] *= (180 / M_PI);

    // Find maximum absolute angle error
    max_err = *minmax.second;
}

// Reverse the array if CCW is set
void ADMTController::reverseArray(vector<double>& PANG, bool CCW) {
    if (CCW) {
        reverse(PANG.begin(), PANG.end());
    }
}

// Randomize the array if circshiftData is set
void ADMTController::randomizeArray(vector<double>& PANG, bool circshiftData) {
    if (circshiftData) {
        int shift = rand() % PANG.size();
        rotate(PANG.begin(), PANG.begin() + shift, PANG.end());
    }
}

// Perform FFT on angle errors
void ADMTController::performFFT(const vector<double>& angle_errors, int samplesPerCycle, int cycles, vector<double>& angle_errors_fft, vector<double>& angle_errors_fft_phase) {
    typedef complex<double> cx;
    cx fft_in[angle_errors.size()];
    cx fft_out[angle_errors.size()];

    for (size_t i = 0; i < angle_errors.size(); ++i) {
        fft_in[i] = cx(angle_errors[i], 0);
    }

    fft(fft_in, fft_out, 8);

    for (size_t i = 0; i < angle_errors.size(); ++i) {
        angle_errors_fft[i] = sqrt(pow(fft_out[i].real() / angle_errors.size(), 2) + pow(-fft_out[i].imag() / angle_errors.size(), 2)) * 2;
        angle_errors_fft_phase[i] = atan2(fft_out[i].imag(), fft_out[i].real());
    }
}

// Extract harmonic magnitudes
void ADMTController::extractHarmonics(const vector<double>& angle_errors_fft, int cycles, double& H1Mag, double& H2Mag, double& H3Mag, double& H8Mag) {
    H1Mag = angle_errors_fft[cycles];
    H2Mag = angle_errors_fft[2 * cycles];
    H3Mag = angle_errors_fft[3 * cycles];
    H8Mag = angle_errors_fft[8 * cycles];
}

// Extract harmonic phases
void ADMTController::extractPhases(const vector<double>& angle_errors_fft_phase, int cycles, double& H1Phase, double& H2Phase, double& H3Phase, double& H8Phase) {
    H1Phase = (180 / M_PI) * (angle_errors_fft_phase[cycles]);
    H2Phase = (180 / M_PI) * (angle_errors_fft_phase[2 * cycles]);
    H3Phase = (180 / M_PI) * (angle_errors_fft_phase[3 * cycles]);
    H8Phase = (180 / M_PI) * (angle_errors_fft_phase[8 * cycles]);
}

// Apply corrections to the data
void ADMTController::applyCorrections(const vector<double>& PANG, double H1Mag, double H2Mag, double H3Mag, double H8Mag, double H1Phase, double H2Phase, double H3Phase, double H8Phase, vector<double>& HXcorrection) {
    for (size_t i = 0; i < PANG.size(); ++i) {
        HXcorrection[i] = PANG[i] - (H1Mag * sin(M_PI / 180 * (PANG[i]) + M_PI / 180 * (H1Phase)) +
                                     H2Mag * sin(2 * M_PI / 180 * (PANG[i]) + M_PI / 180 * (H2Phase)) +
                                     H3Mag * sin(3 * M_PI / 180 * (PANG[i]) + M_PI / 180 * (H3Phase)) +
                                     H8Mag * sin(8 * M_PI / 180 * (PANG[i]) + M_PI / 180 * (H8Phase)));
    }
}

// Derive register-compatible values for HMAG and HPHASE
void ADMTController::deriveRegisterValues(double H1Mag, double H2Mag, double H3Mag, double H8Mag, double H1Phase, double H2Phase, double H3Phase, double H8Phase, int& HAR_MAG_1, int& HAR_MAG_2, int& HAR_MAG_3, int& HAR_MAG_8, int& HAR_PHASE_1, int& HAR_PHASE_2, int& HAR_PHASE_3, int& HAR_PHASE_8) {
    double mag_scale_factor_11bit = 11.2455 / (1 << 11);
    double mag_scale_factor_8bit = 1.40076 / (1 << 8);
    HAR_MAG_1 = (int)(H1Mag / mag_scale_factor_11bit) & (0x7FF); // 11 bit 
    HAR_MAG_2 = (int)(H2Mag / mag_scale_factor_11bit) & (0x7FF); // 11 bit
    HAR_MAG_3 = (int)(H3Mag / mag_scale_factor_8bit) & (0xFF); // 8 bit
    HAR_MAG_8 = (int)(H8Mag / mag_scale_factor_8bit) & (0xFF);  // 8 bit

    double pha_scale_factor_12bit = 360.0 / (1 << 12);
    HAR_PHASE_1 = (int)(H1Phase / pha_scale_factor_12bit) & (0xFFF); // 12bit number
    HAR_PHASE_2 = (int)(H2Phase / pha_scale_factor_12bit) & (0xFFF); // 12bit number
    HAR_PHASE_3 = (int)(H3Phase / pha_scale_factor_12bit) & (0xFFF); // 12bit number
    HAR_PHASE_8 = (int)(H8Phase / pha_scale_factor_12bit) & (0xFFF); // 12bit number
}

// Calculate error magnitude and phase results
void ADMTController::calculateErrorResults(const vector<double>& angle_errors_fft, vector<double>& ErrorMagnitudeResult, vector<double>& ErrorPhaseResult) {
    for (size_t i = 0; i < angle_errors_fft.size(); ++i) {
        ErrorMagnitudeResult[i] = 2 * abs(angle_errors_fft[i]);
        ErrorPhaseResult[i] = angle_errors_fft[i];
    }
}