<template>
  <div class="visual-container mb-5">
    <svg viewBox="0 0 100 100" class="imu-svg">
      <!-- Static Outer Housing -->
      <circle cx="50" cy="50" r="48" fill="none" stroke="#f5f5f5" stroke-width="2" />
      
      <!-- Rotating Compass Card -->
      <!-- We negate the yaw so the card rotates correctly against the fixed red needle -->
      <g :style="yawStyle">
        <text x="50" y="20" text-anchor="middle" font-size="10" font-weight="bold" fill="black">N</text>
        <text x="80" y="53" text-anchor="middle" font-size="10" fill="black">E</text>
        <text x="50" y="87" text-anchor="middle" font-size="10" fill="black">S</text>
        <text x="20" y="53" text-anchor="middle" font-size="10" fill="black">W</text>
        
        <!-- Center crosshair for pro look -->
        <line x1="50" y1="25" x2="50" y2="75" stroke="#eee" stroke-width="1" />
        <line x1="25" y1="50" x2="75" y2="50" stroke="#eee" stroke-width="1" />
      </g>
      
      <!-- Static Heading Pointer (The "Lubber Line") -->
      <path d="M 50 10 L 45 25 L 55 25 Z" fill="red" />
    </svg>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue'

const props = defineProps({
  yaw: {
    type: Number,
    required: true
  }
})

// Track the "infinite" rotation value
const internalYaw = ref(0)
const lastRawYaw = ref(0)

watch(() => props.yaw, (newYaw) => {
  let delta = newYaw - lastRawYaw.value;

  // Shortest path logic: 
  // If the jump is > 180, it means we crossed the 0/360 boundary
  if (delta > 180) {
    delta -= 360;
  } else if (delta < -180) {
    delta += 360;
  }

  internalYaw.value += delta;
  lastRawYaw.value = newYaw;
}, { immediate: true })

const yawStyle = computed(() => ({
  // We negate the internal cumulative value
  transform: `rotate(${-internalYaw.value}deg)`,
  transformOrigin: '50px 50px',
  transition: 'transform 0.1s linear'
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
