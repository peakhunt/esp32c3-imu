<template>
  <div class="visual-container mb-5">
    <svg viewBox="0 0 100 100" class="imu-svg">
      <!-- Static Reference (The Ground/Flat line) -->
      <line x1="10" y1="50" x2="90" y2="50" stroke="#eee" stroke-width="2" stroke-dasharray="4" />
      
      <!-- Static Housing Circle -->
      <circle cx="50" cy="50" r="48" fill="none" stroke="#f5f5f5" stroke-width="2" />

      <!-- THE MOVING WING (Horizon Line) -->
      <!-- +45 rotate right (CW), -45 rotate left (CCW) -->
      <g :style="rollStyle">
        <line x1="15" y1="50" x2="85" y2="50" stroke="#485fc7" stroke-width="8" stroke-linecap="round" />
        <!-- Needle pointer -->
        <path d="M 50 15 L 50 5" stroke="#485fc7" stroke-width="4" fill="none" />
      </g>
      
      <!-- Center Pivot -->
      <circle cx="50" cy="50" r="4" fill="#485fc7" />
    </svg>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  roll: {
    type: Number,
    required: true
  }
})

const rollStyle = computed(() => ({
  transform: `rotate(${props.roll}deg)`,
  transformOrigin: '50px 50px',
  // transition: 'transform 0.1s linear'
}))
</script>

<style scoped>
.visual-container { 
  width: 120px; 
  height: 120px; 
  margin: 0 auto; 
  display: flex; 
  align-items: center; 
  justify-content: center; 
}
.imu-svg { 
  width: 100%; 
  height: 100%; 
  overflow: visible; 
}
</style>
