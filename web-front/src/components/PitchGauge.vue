<template>
  <div class="visual-container mb-5 horizon-box">
    <!-- Changed viewBox to 100x90 for 1:0.5 ratio -->
    <svg viewBox="0 0 100 90" class="imu-svg" preserveAspectRatio="xMidYMid slice">
      <!-- Pitch Ladder & Horizon Group -->
      <g :style="horizonStyle">
        <!-- Sky & Ground: Oversized to cover movement -->
        <rect x="-50" y="-100" width="200" height="145" fill="#87CEEB" /> 
        <rect x="-50" y="45" width="200" height="145" fill="#8B4513" />
        
        <!-- Horizon Line at the center (45) -->
        <line x1="0" y1="45" x2="100" y2="45" stroke="white" stroke-width="2" />
        
        <!-- Pitch Marks: 20 degrees apart (10 units) -->
        <line x1="40" y1="35" x2="60" y2="35" stroke="white" stroke-width="1" /> <!-- +20 deg -->
        <line x1="40" y1="55" x2="60" y2="55" stroke="white" stroke-width="1" /> <!-- -20 deg -->
      </g>
      
      <!-- Static Plane Reference -->
      <path d="M 35 45 L 45 45 M 55 45 L 65 45 M 50 40 L 50 50" stroke="yellow" stroke-width="2" />
    </svg>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  pitch: { type: Number, required: true }
})

const horizonStyle = computed(() => ({
  // Now exactly 0.5. Positive pitch (nose up) moves horizon down.
  transform: `translateY(${props.pitch * 0.5}px)`,
  transition: 'transform 0.1s linear',
  transformOrigin: 'center'
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
  position: relative; 
}
.horizon-box { 
  border-radius: 50%; 
  overflow: hidden; 
  border: 2px solid #333; 
}
.imu-svg { 
  width: 100%; 
  height: 100%; 
  overflow: visible; 
}
</style>
