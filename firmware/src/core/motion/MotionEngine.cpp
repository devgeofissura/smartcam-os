#include "MotionEngine.h"
#include "AxisController.h"

static MotionEngine* s_instance = nullptr;

static const int MAX_AXES = 4;

MotionEngine::MotionEngine()
    : m_axisCount(0) {
    for (int i = 0; i < MAX_AXES; i++) {
        m_axes[i] = nullptr;
    }
}

MotionEngine::~MotionEngine() {
    stop();
}

bool MotionEngine::begin() {
    s_instance = this;
    return true;
}

void MotionEngine::update() {
}

bool MotionEngine::stop() {
    for (int i = 0; i < m_axisCount; i++) {
        if (m_axes[i]) {
            m_axes[i]->stop();
            delete m_axes[i];
            m_axes[i] = nullptr;
        }
    }
    m_axisCount = 0;
    s_instance = nullptr;
    return true;
}

bool MotionEngine::addAxis(const AxisConfig& config) {
    if (m_axisCount >= MAX_AXES) return false;

    AxisController* axis = new AxisController();
    if (!axis) return false;

    if (!axis->init(config)) {
        delete axis;
        return false;
    }

    m_axes[m_axisCount++] = axis;
    return true;
}

bool MotionEngine::removeAxis(int index) {
    if (index < 0 || index >= m_axisCount) return false;
    if (m_axes[index]) {
        m_axes[index]->stop();
        delete m_axes[index];
        m_axes[index] = nullptr;
    }
    for (int i = index; i < m_axisCount - 1; i++) {
        m_axes[i] = m_axes[i + 1];
    }
    m_axes[--m_axisCount] = nullptr;
    return true;
}

int MotionEngine::getAxisCount() const {
    return m_axisCount;
}

IMotionController* MotionEngine::getAxis(int index) {
    if (index < 0 || index >= m_axisCount) return nullptr;
    return m_axes[index];
}

bool MotionEngine::moveTo(int axisIndex, long position) {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) return false;
    return m_axes[axisIndex]->moveTo(position);
}

bool MotionEngine::moveRelative(int axisIndex, long steps) {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) return false;
    return m_axes[axisIndex]->moveRelative(steps);
}

bool MotionEngine::stopAxis(int axisIndex) {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) return false;
    return m_axes[axisIndex]->stop();
}

bool MotionEngine::homeAxis(int axisIndex) {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) return false;
    return m_axes[axisIndex]->home();
}

bool MotionEngine::enableAxis(int axisIndex, bool enable) {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) return false;
    return m_axes[axisIndex]->enable(enable);
}

bool MotionEngine::setProfile(int axisIndex, const MotionProfile& profile) {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) return false;
    m_axes[axisIndex]->setSpeed(profile.maxSpeed);
    m_axes[axisIndex]->setAcceleration(profile.acceleration);
    return true;
}

AxisState MotionEngine::getAxisState(int axisIndex) const {
    if (axisIndex < 0 || axisIndex >= m_axisCount || !m_axes[axisIndex]) {
        return AxisState();
    }
    return m_axes[axisIndex]->getState();
}
