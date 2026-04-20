<template>
  <div class="cube-container">
    <div class="cube-scene">
      <div class="cube" :style="{ transform: rotationMap[target] || rotationMap['+Z'] }">
        <!-- Manually define faces to ensure classes match CSS 1:1 -->
        <div class="face plus-z"  :class="{ 'is-active': target === '+Z', 'is-done': doneSides.includes('+Z') }"><span>+Z</span></div>
        <div class="face minus-z" :class="{ 'is-active': target === '-Z', 'is-done': doneSides.includes('-Z') }"><span>-Z</span></div>
        <div class="face plus-x"  :class="{ 'is-active': target === '+X', 'is-done': doneSides.includes('+X') }"><span>+X</span></div>
        <div class="face minus-x" :class="{ 'is-active': target === '-X', 'is-done': doneSides.includes('-X') }"><span>-X</span></div>
        <div class="face plus-y"  :class="{ 'is-active': target === '+Y', 'is-done': doneSides.includes('+Y') }"><span>+Y</span></div>
        <div class="face minus-y" :class="{ 'is-active': target === '-Y', 'is-done': doneSides.includes('-Y') }"><span>-Y</span></div>
      </div>
    </div>
  </div>
</template>

<script setup>
const props = defineProps({
  target: { type: String, default: '+Z' }, // The side we WANT them to calibrate
  doneSides: { type: Array, default: () => [] } // Sides already calibrated (Green)
})

const faces = ['+X', '-X', '+Y', '-Y', '+Z', '-Z']

// These rotations bring the specific face to the FRONT
const rotationMap = {
  '+Z': 'rotateX(0deg) rotateY(0deg)',
  '-Z': 'rotateX(180deg) rotateY(0deg)',
  '+X': 'rotateX(0deg) rotateY(-90deg)',
  '-X': 'rotateX(0deg) rotateY(90deg)',
  '+Y': 'rotateX(-90deg) rotateY(0deg)',
  '-Y': 'rotateX(90deg) rotateY(0deg)',
}
</script>

<style scoped>
.cube-container {
  perspective: 1000px;
  width: 100%;
  height: 180px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.cube-scene {
  width: 80px;
  height: 80px;
  transform-style: preserve-3d;
  /* This tilt is what lets you see the 3D 'box' shape */
  transform: rotateX(-20deg) rotateY(25deg); 
}

.cube {
  width: 100%;
  height: 100%;
  position: relative;
  transform-style: preserve-3d;
  transition: transform 0.6s ease-in-out;
}

.face {
  position: absolute;
  width: 80px;
  height: 80px;
  background: rgba(40, 40, 40, 0.9); /* Slightly transparent to see the 'box' */
  border: 2px solid #555;
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: monospace;
  font-weight: bold;
  font-size: 1.2rem;
  color: #aaa;
  backface-visibility: hidden; /* Hide the 'inside' of the box */
}

/* THE FOLDING LOGIC */
.plus-z  { transform: translateZ(40px); }
.minus-z { transform: rotateY(180deg) translateZ(40px); }
.plus-x  { transform: rotateY(90deg)  translateZ(40px); }
.minus-x { transform: rotateY(-90deg) translateZ(40px); }
.plus-y  { transform: rotateX(90deg)  translateZ(40px); }
.minus-y { transform: rotateX(-90deg) translateZ(40px); }

/* ACTIVE & DONE STATES */
.face.is-active {
  background: rgba(0, 209, 178, 0.4);
  border-color: #00d1b2;
  color: #fff;
}
.face.is-done {
  background: #48c78e !important;
  border-color: #fff;
  color: #fff;
}

face.is-active {
  background: rgba(0, 209, 178, 0.6);
  border-color: #fff;
  color: #fff;
  /* Add this pulse */
  animation: pulse-active 1.5s infinite ease-in-out;
}

@keyframes pulse-active {
  0% { box-shadow: 0 0 5px rgba(0, 209, 178, 0.4); }
  50% { box-shadow: 0 0 25px rgba(0, 209, 178, 0.9); }
  100% { box-shadow: 0 0 5px rgba(0, 209, 178, 0.4); }
}
</style>
