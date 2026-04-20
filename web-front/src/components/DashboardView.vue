<template>
  <div>
    <!-- GAUGES SECTION -->
    <div class="columns is-multiline is-variable is-2">
      <div class="column is-4-desktop is-12-mobile" v-for="type in ['ROLL', 'PITCH', 'YAW']" :key="type">
        <div class="card has-background-white has-text-centered shadow-card">
          <div class="card-content px-2 py-5">
            <p class="heading has-text-weight-bold has-text-black mb-4">{{ type }}</p>
            <component :is="getGauge(type)" :[type.toLowerCase()]="state[type.toLowerCase()]" />
            <p class="title is-2 has-text-black">{{ state[type.toLowerCase()].toFixed(type === 'YAW' ? 0 : 1) }}°</p>
          </div>
        </div>
      </div>
    </div>

    <!-- TELEMETRY CHART -->
    <div class="column is-12 mt-5 p-0">
      <div class="card has-background-white shadow-card">
        <div class="card-content">
          <p class="heading has-text-weight-bold has-text-black mb-4">REAL-TIME TELEMETRY</p>
          <!-- Chart.js needs a relative container with a height -->
          <div class="chart-container" style="position: relative; height: 250px; width: 100%;">
            <canvas ref="chartCanvas"></canvas>
          </div>
        </div>
      </div>
    </div>

    <!-- 3D ATTITUDE VIEW -->
    <div class="column is-12 mt-5 p-0">
      <div class="card has-background-white shadow-card">
        <div class="card-content">
          <Attitude3D :roll="state.roll" :pitch="state.pitch" :yaw="state.yaw" />
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue'
import { useIMUStore } from '../store/imuStore'
import { Chart, registerables } from 'chart.js'
Chart.register(...registerables)

import Attitude3D from './Attitude3D.vue'
import PitchGauge from './PitchGauge.vue'
import RollGauge from './RollGauge.vue'
import YawGauge from './YawGauge.vue'

const { state } = useIMUStore()
const chartCanvas = ref(null)
let chartInstance = null

const max_points = 1000
const getGauge = (type) => ({ ROLL: RollGauge, PITCH: PitchGauge, YAW: YawGauge }[type])

watch(() => state.roll, (newRoll) => {
  updateChart(state.roll, state.pitch, state.yaw)
})

const updateChart = (r, p, y) => {
  const datasets = chartInstance.data.datasets;
  const values = [r, p, y];

  // 2. Update each of the 3 series
  values.forEach((val, i) => {
    datasets[i].data.push(val);
    if (datasets[i].data.length > max_points) datasets[i].data.shift();
  });

  // 3. 'none' mode skips CPU-intensive animations for 50Hz streaming
  chartInstance.update('none');
}

defineExpose({ updateChart })

onMounted(() => {
  const ctx = chartCanvas.value.getContext('2d');
  
  chartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      // Create empty slots for the timeline
      labels: Array.from({ length: max_points }, (_, i) => i),
      datasets: [
        { label: 'Roll',  yAxisID: 'y', borderColor: '#485fc7', data: Array(max_points).fill(0), pointRadius: 0, borderWidth: 2 },
        { label: 'Pitch', yAxisID: 'y', borderColor: '#ff3860', data: Array(max_points).fill(0), pointRadius: 0, borderWidth: 2 },
        { label: 'Yaw',   yAxisID: 'yYaw', borderColor: '#2dff27', data: Array(max_points).fill(0), pointRadius: 0, borderWidth: 2 }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false, 
      scales: {
        x: { display: false },
        // Left Axis for Roll/Pitch
        y: { 
          type: 'linear', position: 'left',
          min: -180, max: 180,
          grid: { color: '#f0f0f0' },
          ticks: { callback: (v) => v + '°' }
        },
        // Right Axis for Yaw (Heading)
        yYaw: {
          type: 'linear', position: 'right',
          min: 0, max: 360,
          grid: { drawOnChartArea: false }, // Don't double-up the grid lines
          ticks: { callback: (v) => v + '°', color: '#2dff27' }
        }
      },
      plugins: {
        legend: { display: true, labels: { color: '#2c3e50', font: { weight: '600' } } }
      }
    }
  });
})

onUnmounted(() => {
  if (chartInstance) chartInstance.destroy()
})
</script>

<style scoped>
.chart-container {
  width: 100%;
  background: #fff;
}
.shadow-card {
  box-shadow: 0 2px 15px rgba(0,0,0,0.05);
  border-radius: 8px;
}
</style>
