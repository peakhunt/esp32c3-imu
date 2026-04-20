<template>
  <div class="calibration-view px-2">
    
    <!-- ACCELEROMETER SECTION -->
    <div class="card shadow-card mb-4 has-background-white">
      <div class="card-content p-3">
        <p class="heading has-text-weight-bold has-text-grey-light mb-3">ACCELEROMETER (g)</p>
        <div class="columns is-mobile is-multiline is-vcentered is-variable is-1">
          <div class="column is-2-tablet is-6-mobile border-right-tablet">
            <div v-for="axis in ['x','y','z']" :key="axis" class="is-flex mb-1 pr-2">
              <span class="is-size-6" :style="{ width: '30px', color: getAxisColor(axis) }">{{ axis.toUpperCase() }}:</span>
              <span class="is-family-monospace is-size-6 has-text-weight-bold">{{ imuStore.state.accel[axis].toFixed(2) }}</span>
            </div>
          </div>
          <div class="column is-4-tablet is-6-mobile has-text-centered border-left-tablet">
            <div class="mag-visual-wrapper" 
                 :style="{ height: chart_height + 'px', width: chart_height + 'px' }">
              <AccelCubeView :target="currentTarget" :done-sides="completedSides" />
            </div>
          </div>
          <div class="column is-6-tablet is-12-mobile mt-3-mobile">
             <!-- Chart.js needs a relative container and a canvas -->
             <div class="chart-container" :style="{ height: chart_height + 'px', position: 'relative' }">
                <canvas ref="accelChartEl"></canvas>
             </div>
          </div>
        </div>
        <!-- ACCEL BUTTON SECTION -->
        <div class="mt-4">
          <button 
             class="button is-primary is-small is-fullwidth has-text-weight-bold"
             :disabled="!detectedSide || imuStore.state.isCalibrating"
             @click="apiCalibrate('accel', detectedSide)"
             >
             <template v-if="imuStore.state.isCalibrating && currentTarget === detectedSide">
               RECORDING {{ detectedSide }}...
             </template>
            <template v-else-if="detectedSide">
              {{ calState.accel[detectedSide] ? 'RE-CAPTURE ' + detectedSide : 'CAPTURE ' + detectedSide }}
            </template>
            <template v-else>
              LEVEL SENSOR TO START
            </template>
          </button>

          <div class="is-flex is-align-items-center is-justify-content-center" style="gap: 12px;">
            <button class="button is-ghost p-0 h-auto is-flex"
                    @click="resetAccelCal"
                    style="min-height: unset; border: none; background: none;"
                    title="Reset All Progress">
              <Icon :icon="trashCanOutline"
                    class="trash-icon" 
                    style="font-size: 1.2rem;" />
            </button>
              <div class="is-flex" style="gap: 5px;">
                <div v-for="side in ['+X','-X','+Y','-Y','+Z','-Z']" :key="side"
                     :class="['tag', calState.accel[side] ? 'is-success' : 'is-light']"
                     style="width: 35px; height: 8px; padding: 0; border-radius: 2px;">
                </div>
              </div>
          </div>
        </div>
      </div>
    </div>

    <!-- GYROSCOPE SECTION -->
    <div class="card shadow-card mb-4 has-background-white">
      <div class="card-content p-3">
        <p class="heading has-text-weight-bold has-text-grey-light mb-3">GYROSCOPE (°/s)</p>
        <div class="columns is-mobile is-multiline is-vcentered is-variable is-1">
          <div class="column is-2-tablet is-6-mobile border-right-tablet">
            <div v-for="axis in ['x','y','z']" :key="axis" class="is-flex mb-1 pr-2">
              <span class="is-size-6" :style="{ width: '30px', color: getAxisColor(axis) }">{{ axis.toUpperCase() }}:</span>
              <span class="is-family-monospace is-size-6 has-text-weight-bold">{{ imuStore.state.gyro[axis].toFixed(2) }}</span>
            </div>
          </div>
          <div class="column is-4-tablet is-6-mobile has-text-centered border-left-tablet">
            <div class="mag-visual-wrapper"
                 :style="{ height: chart_height + 'px', width: chart_height + 'px' }">
              <GyroStillnessView :progress="calProgress" :calibrating="isCalibrating" :noise-level="gyroMagnitude" :threshold="0.5"/>
            </div>
          </div>
          <div class="column is-6-tablet is-12-mobile mt-3-mobile">
             <div class="chart-container" :style="{ height: chart_height + 'px', position: 'relative' }">
                <canvas ref="gyroChartEl"></canvas>
             </div>
          </div>
        </div>
        <button class="button is-primary is-small is-fullwidth has-text-weight-bold mt-4"
                :disabled="imuStore.state.isCalibrating"
                @click="apiCalibrate('gyro')">
          {{ imuStore.state.isCalibrating ? 'CALIBRATING BIAS...' : 'EXECUTE BIAS' }}
        </button>
      </div>
    </div>

    <!-- MAGNETOMETER SECTION -->
    <div class="card shadow-card mb-4 has-background-white">
      <div class="card-content p-3">
        <p class="heading has-text-weight-bold has-text-grey-light mb-3">MAGNETOMETER (µT)</p>
        <div class="columns is-mobile is-multiline is-vcentered is-variable is-1">
          <div class="column is-2-tablet is-6-mobile border-right-tablet">
            <div v-for="axis in ['x','y','z']" :key="axis" class="is-flex mb-1 pr-2">
              <span class="is-size-6" :style="{ width: '30px', color: getAxisColor(axis) }">{{ axis.toUpperCase() }}:</span>
              <span class="is-family-monospace is-size-6 has-text-weight-bold">{{ imuStore.state.mag[axis].toFixed(1) }}</span>
            </div>
          </div>
          <div class="column is-4-tablet is-6-mobile has-text-centered border-left-tablet">
            <div class="mag-visual-wrapper"
                 :style="{ height: chart_height + 'px', width: chart_height + 'px' }">
              <MagCloudView ref="magCloudRef" />
            </div>
          </div>
          <div class="column is-6-tablet is-12-mobile mt-3-mobile">
             <div class="chart-container" :style="{ height: chart_height + 'px', position: 'relative' }">
                <canvas ref="magChartEl"></canvas>
             </div>
          </div>
        </div>
        <button class="button is-primary is-small is-fullwidth has-text-weight-bold mt-4" 
                :disabled="imuStore.state.isCalibrating" 
                @click="apiCalibrate('mag')">
          {{ imuStore.state.isCalibrating ? 'HARDWARE BUSY' : 'RESET CLOUD' }}
        </button>
      </div>
    </div>

    <!-- SYSTEM CALIBRATION SETTINGS SECTION -->
    <div class="card shadow-card mb-4 has-background-white">
      <div class="card-content p-3">
        <div class="is-flex is-justify-content-between is-align-items-center mb-2">
          <p class="heading has-text-weight-bold has-text-grey-light mb-0">ACTIVE HARDWARE PARAMETERS (%.3f)</p>
          <button class="button is-ghost is-small p-0 h-auto"
                  @click="imuStore.fetchSettings()"
                  style="min-height: unset; border: none; background: none;">
            <Icon :icon="refreshIcon" class="mr-1" style="font-size: 1rem; color: #b5b5b5;" />
            <span class="is-size-7 has-text-grey">RELOAD</span>
          </button>
        </div>

        <div class="columns is-mobile is-multiline is-variable is-1">
          <!-- Accel Row -->
          <div class="column is-6-mobile is-4-tablet py-1">
            <p class="is-size-7 has-text-grey-light">ACCEL OFF</p>
            <div class="is-family-monospace is-size-7 has-text-weight-bold">
              [{{ imuStore.state.settings.calibration.accel_off.map(v => v.toFixed(3)).join(', ') }}]
            </div>
          </div>
          <div class="column is-6-mobile is-4-tablet py-1">
            <p class="is-size-7 has-text-grey-light">ACCEL SCALE</p>
            <div class="is-family-monospace is-size-7 has-text-weight-bold">
              [{{ imuStore.state.settings.calibration.accel_scale.map(v => v.toFixed(3)).join(', ') }}]
            </div>
          </div>

          <!-- Gyro/Mag Row -->
          <div class="column is-6-mobile is-4-tablet py-1">
            <p class="is-size-7 has-text-grey-light">GYRO OFF</p>
            <div class="is-family-monospace is-size-7 has-text-weight-bold">
              [{{ imuStore.state.settings.calibration.gyro_off.map(v => v.toFixed(3)).join(', ') }}]
            </div>
          </div>
          <div class="column is-6-mobile is-4-tablet py-1">
            <p class="is-size-7 has-text-grey-light">MAG BIAS</p>
            <div class="is-family-monospace is-size-7 has-text-weight-bold">
              [{{ imuStore.state.settings.calibration.mag_bias.map(v => v.toFixed(3)).join(', ') }}]
            </div>
          </div>
          <div class="column is-6-mobile is-4-tablet py-1">
            <p class="is-size-7 has-text-grey-light">MAG SCALE</p>
            <div class="is-family-monospace is-size-7 has-text-weight-bold">
              [{{ imuStore.state.settings.calibration.mag_scale.map(v => v.toFixed(3)).join(', ') }}]
            </div>
          </div>

          <!-- Declination -->
          <div class="column is-6-mobile is-4-tablet py-1">
            <p class="is-size-7 has-text-grey-light">DECLINATION</p>
            <div class="is-family-monospace is-size-6 has-text-weight-bold has-text-success">
              {{ imuStore.state.settings.imu.mag_declination.toFixed(3) }}°
            </div>
          </div>
        </div>
      </div>
    </div>

  </div>
</template>

<script setup>
import { Icon } from '@iconify/vue'
import trashCanOutline from '@iconify-icons/mdi/trash-can-outline'
import refreshIcon from '@iconify-icons/mdi/refresh'
import { reactive, onMounted, onUnmounted, ref, computed, watch } from 'vue'
import { useIMUStore } from '../store/imuStore'
import MagCloudView from './MagCloudView.vue'
import GyroStillnessView from './GyroStillnessView.vue'
import AccelCubeView from './AccelCubeView.vue'

// 1. Switch to Chart.js
import { Chart, registerables } from 'chart.js'
Chart.register(...registerables)

const imuStore = useIMUStore()
const magCloudRef = ref(null)

// Refs for the <canvas> elements
const accelChartEl = ref(null)
const gyroChartEl = ref(null)
const magChartEl = ref(null)

const calState = reactive({ accel: { '+X': false, '-X': false, '+Y': false, '-Y': false, '+Z': false, '-Z': false } })
const max_data = 500

const windowWidth = ref(window.innerWidth)
const updateWidth = () => { windowWidth.value = window.innerWidth }

const chart_height = computed(() => windowWidth.value < 769 ? 120 : 250)
const getAxisColor = (axis) => ({ x: '#3246a3', y: '#cc002e', z: '#1ca318' }[axis])

const currentTarget = ref('+Z')
const completedSides = ref([])
const chartInstances = {}

watch(() => imuStore.state.gyro, (newGyro) => {
  updateCalibrationCharts()
})

// 2. Updated Update Logic (The Surgical Fix)
const updateCalibrationCharts = () => {
  ['gyro', 'accel', 'mag'].forEach(id => {
    const chart = chartInstances[id]
    const sensor = imuStore.state[id]
    if (!chart) return

    // Update each of the 3 series (X, Y, Z)
    const datasets = chart.data.datasets
    const values = [sensor.x, sensor.y, sensor.z]

    values.forEach((val, i) => {
      datasets[i].data.push(val)
      if (datasets[i].data.length > max_data) datasets[i].data.shift()
    })

    // 'none' mode is critical for 50Hz performance
    chart.update('none')
  })

  if (magCloudRef.value?.addPoint) {
    magCloudRef.value.addPoint(imuStore.state.mag.x, imuStore.state.mag.y, imuStore.state.mag.z)
  }
}

defineExpose({ updateCalibrationCharts })

onMounted(() => {
  window.addEventListener('resize', updateWidth)

  const commonOptions = (unit) => ({
    responsive: true,
    maintainAspectRatio: false,
    animation: false,
    elements: { point: { radius: 0 }, line: { borderWidth: 2, tension: 0 } },
    scales: {
      x: { display: false },
      y: { 
        grid: { color: '#f0f0f0' },
        ticks: { 
          callback: (v) => v + unit,
          color: '#2c3e50',
          font: { weight: '600' }
        },
        // Buffer range logic similar to your old uPlot opts
        suggestedMin: -1.5,
        suggestedMax: 1.5
      }
    },
    plugins: { legend: { display: false } }
  })

  const initChart = (el, unit) => {
    return new Chart(el.getContext('2d'), {
      type: 'line',
      data: {
        labels: Array.from({ length: max_data }, (_, i) => i),
        datasets: [
          { borderColor: '#0000ff', data: Array(max_data).fill(0) },
          { borderColor: '#ff0000', data: Array(max_data).fill(0) },
          { borderColor: '#00ff00', data: Array(max_data).fill(0) }
        ]
      },
      options: commonOptions(unit)
    })
  }

  chartInstances.gyro = initChart(gyroChartEl.value, "°/s")
  chartInstances.accel = initChart(accelChartEl.value, "g")
  chartInstances.mag = initChart(magChartEl.value, "µT")
})

// Keep the internal Chart.js resize handling in sync with your height computed
watch(chart_height, () => {
  Object.values(chartInstances).forEach(chart => {
    if (chart) chart.resize()
  })
})

onUnmounted(() => {
  window.removeEventListener('resize', updateWidth)
  Object.values(chartInstances).forEach(i => i?.destroy?.())
})

// --- REST OF YOUR ORIGINAL LOGIC UNTOUCHED ---
const runCal = (type, axis = null) => {
  if (type === 'mag' && magCloudRef.value) magCloudRef.value.reset()
  if (type === 'accel' && axis) {
    currentTarget.value = axis
    calState.accel[axis] = true
    if (!completedSides.value.includes(axis)) completedSides.value.push(axis)
  }
}

const calProgress = ref(0); const isCalibrating = ref(false)
const gyroMagnitude = computed(() => {
  const g = imuStore.state.gyro
  return Math.sqrt(g.x**2 + g.y**2 + g.z**2)
})

const runGyroCal = () => {
  imuStore.setCalibrating(true);
  setTimeout(() => {
    imuStore.setCalibrating(false);
  }, 5000);
}

const detectedSide = computed(() => {
  const a = imuStore.state.accel;
  const threshold = 0.2; 
  if (a.z < (-1.0 + threshold)) return '+Z';
  if (a.z > (1.0 - threshold))  return '-Z';
  if (a.x < (-1.0 + threshold)) return '+X';
  if (a.x > (1.0 - threshold))  return '-X';
  if (a.y < (-1.0 + threshold)) return '+Y';
  if (a.y > (1.0 - threshold))  return '-Y';
  return null;
});

watch(detectedSide, (newSide) => {
  if (newSide) currentTarget.value = newSide
})

const apiCalibrate = async (sensorType, axis = null) => {
  imuStore.setCalibrating(true);
  let url = `/api/calibrate/${sensorType}`;
  if (axis) {
    url += `?side=${encodeURIComponent(axis)}`;
    if (sensorType === 'accel' && completedSides.value.length === 0) url += `&reset=1`;
  }
  try {
    const controller = new AbortController();
    const timeoutMs = (sensorType === 'accel') ? 30000 : 90000;
    const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
    const response = await fetch(url, { method: 'POST', signal: controller.signal });
    clearTimeout(timeoutId);
    if (!response.ok) throw new Error(`Hardware returned ${response.status}`);
    if (sensorType === 'accel' && axis) {
      if (!completedSides.value.includes(axis)) completedSides.value.push(axis);
      calState.accel[axis] = true;
    }
    if (sensorType === 'accel' && completedSides.value.length === 6) {
      const finishResponse = await fetch('/api/calibrate/accel/finish', { method: 'POST' });
      alert("Calibration Saved!");
    }
  } catch (err) {
    alert(`Error: ${err.message}`);
  } finally {
    imuStore.setCalibrating(false);
  }
}

const resetAccelCal = () => {
  if (confirm("Clear all captured sides?")) {
    completedSides.value = [];
    Object.keys(calState.accel).forEach(k => calState.accel[k] = false);
  }
}
</script>

<style scoped>
/* The stable house for Chart.js */
.chart-container {
  display: block;
  width: 100%;
  position: relative;
  background: #fafafa;
  border-radius: 4px;
  overflow: hidden;
}

/* Canvas needs to be block-level to prevent baseline spacing slop */
canvas {
  display: block;
  width: 100%;
  height: 100%;
}

@media screen and (min-width: 769px) {
  .border-right-tablet { border-right: 1px solid #eee; }
  .border-left-tablet { border-left: 1px solid #eee; }
}

@media screen and (max-width: 768px) {
  .mt-3-mobile { margin-top: 0.75rem; padding-top: 0.75rem; border-top: 1px solid #f5f5f5; }
}

.is-family-monospace { font-family: 'Courier New', Courier, monospace; }
.shadow-card { border-radius: 12px; border: 1px solid #efefef; }

.mag-visual-wrapper {
  position: relative;
  /* Remove the var() fallback, Vue :style will override this */
  margin: 0 auto;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
}

.mag-visual-wrapper > * { width: 100%; height: 100%; }

.trash-icon {
  color: #b5b5b5; 
  transition: color 0.2s ease;
  cursor: pointer;
}

.trash-icon:hover {
  color: #ff3860 !important; 
}

.is-flex.is-align-items-center {
  min-height: 24px;
}
</style>
