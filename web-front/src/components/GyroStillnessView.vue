<template>
  <div class="stillness-wrapper">
    <svg viewBox="0 0 100 100" class="stillness-svg">
      <!-- Background Circle (The Track) -->
      <circle class="track" cx="50" cy="50" r="45" />
      
      <!-- Progress Circle -->
      <circle 
        class="progress" 
        cx="50" cy="50" r="45"
        :style="{ strokeDashoffset: offset }"
      />
    </svg>
    
    <!-- Central Status -->
    <div class="status-overlay">
      <!-- Status Text: Always live -->
      <div :class="['status-text', { 'is-vibrating': noiseLevel > threshold }]">
        {{ noiseLevel > threshold ? 'MOVING' : 'STILL' }}
      </div>

      <!-- Progress: Only show number when actually calibrating -->
      <div v-if="calibrating" class="is-size-4 has-text-weight-bold has-text-primary">
        {{ Math.round(progress) }}%
      </div>
      <div v-else class="is-size-7 has-text-grey mt-1">READY</div>
    </div>

  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  progress: { type: Number, default: 0 },    // 0 to 100
  calibrating: { type: Boolean, default: false },
  noiseLevel: { type: Number, default: 0 },  // Live gyro magnitude
  threshold: { type: Number, default: 0.5 } // Max allowed movement
})

// Circular progress logic: Circumference = 2 * PI * R (2 * 3.14 * 45 = 283)
const circumference = 283
const offset = computed(() => {
  return circumference - (props.progress / 100) * circumference
})
</script>

<style scoped>
.stillness-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
}

.stillness-svg {
  width: 80%;
  transform: rotate(-90deg); /* Start at top */
}

circle {
  fill: none;
  stroke-width: 8;
  stroke-linecap: round;
}

.track {
  stroke: #eee;
}

.progress {
  stroke: #00d1b2;
  stroke-dasharray: 283;
  transition: stroke-dashoffset 0.1s linear, stroke 0.3s;
}

.status-overlay {
  position: absolute;
  text-align: center;
}

.status-text {
  font-family: monospace;
  font-weight: bold;
  letter-spacing: 1px;
  transition: color 0.2s;
  /* Default STILL color */
  color: #00d1b2; 
}

.is-vibrating {
  /* MOVING color */
  color: #ff3860 !important; 
  animation: shake 0.15s infinite;
}

@keyframes shake {
  0% { transform: translate(1px, 1px); }
  25% { transform: translate(-1px, -2px); }
  50% { transform: translate(-2px, 1px); }
  100% { transform: translate(1px, -1px); }
}
</style>
