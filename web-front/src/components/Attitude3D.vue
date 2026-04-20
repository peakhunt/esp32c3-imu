<template>
  <div class="card has-background-white shadow-card mt-5">
    <div class="card-content p-2">
      <!-- Header Row: Added flex-wrap and responsive padding -->
      <div class="is-flex is-justify-content-space-between is-justify-content-center-mobile is-align-items-center is-flex-wrap-wrap px-3 pt-2 pb-2">
        
        <!-- Added 'has-text-centered-mobile' and 'is-full-mobile' to force center-stacking -->
        <p class="heading has-text-black has-text-weight-bold mb-1-mobile mr-2-desktop is-full-mobile has-text-centered-mobile">
          3D ATTITUDE (NED)
        </p>
        
        <button class="button is-small is-dark is-rounded has-text-weight-bold" @click="resetHeading">
          RESET HEADING
        </button>
      </div>

      <div class="canvas-wrapper" ref="wrapperRef">
        <!-- HUD Overlay: Fixed to Top-Left -->
        <div class="hud-overlay">
          <div class="hud-line"><span class="hud-label">R:</span> {{ props.roll.toFixed(1) }}°</div>
          <div class="hud-line"><span class="hud-label">P:</span> {{ props.pitch.toFixed(1) }}°</div>
          <div class="hud-line"><span class="hud-label">Y:</span> {{ props.yaw.toFixed(1) }}°</div>
        </div>

        <!-- DYNAMIC 3D LABELS (North, East, Down) -->
        <div class="label-3d n-label" :style="labelStyles.n">N</div>
        <div class="label-3d e-label" :style="labelStyles.e">E</div>
        <div class="label-3d d-label" :style="labelStyles.d">D</div>

        <canvas ref="glCanvas" class="attitude-canvas"></canvas>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, reactive } from 'vue'
import * as twgl from 'twgl.js'

const props = defineProps(['roll', 'pitch', 'yaw'])
const glCanvas = ref(null)
const wrapperRef = ref(null)
const m4 = twgl.m4

const yawOffset = ref(0)
const resetHeading = () => { yawOffset.value = props.yaw }

const labelStyles = reactive({
  n: { transform: 'translate(-100px, -100px)' },
  e: { transform: 'translate(-100px, -100px)' },
  d: { transform: 'translate(-100px, -100px)' }
})

onMounted(() => {
  const gl = glCanvas.value.getContext("webgl")
  const programInfo = twgl.createProgramInfo(gl, [vs, fs])
  const cubeBufferInfo = twgl.primitives.createCubeBufferInfo(gl, 1)
  const lineBufferInfo = twgl.createBufferInfoFromArrays(gl, { position: [0,0,0, 1,0,0] })
  const coneBufferInfo = twgl.primitives.createTruncatedConeBufferInfo(gl, 0.15, 0, 0.4, 12, 1)

  function render() {
    twgl.resizeCanvasToDisplaySize(gl.canvas)
    gl.viewport(0, 0, gl.canvas.width, gl.canvas.height)
    gl.enable(gl.DEPTH_TEST)
    gl.clearColor(0.98, 0.98, 0.98, 1.0)
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT)

    const aspect = gl.canvas.clientWidth / gl.canvas.clientHeight
    const projection = m4.perspective(45 * Math.PI / 180, aspect, 0.1, 50)
    const camera = m4.lookAt([0, 2.5, 13], [0, 0, 0], [0, 1, 0])
    const viewProj = m4.multiply(projection, m4.inverse(camera))

    const displayYaw = props.yaw - yawOffset.value
    let world = m4.identity()
    world = m4.axisRotate(world, [0, 1, 0], -displayYaw * Math.PI / 180)
    world = m4.axisRotate(world, [1, 0, 0], props.pitch * Math.PI / 180)
    world = m4.axisRotate(world, [0, 0, 1], -props.roll * Math.PI / 180)

    const projectLabel = (localPos) => {
      const tip3d = m4.transformPoint(world, localPos)
      const clipSpace = m4.transformPoint(viewProj, tip3d)
      if (clipSpace[2] > 1) return 'translate(-100px, -100px)' // Hide if behind
      const x = (clipSpace[0] * 0.5 + 0.5) * gl.canvas.clientWidth
      const y = (clipSpace[1] * -0.5 + 0.5) * gl.canvas.clientHeight
      return `translate(${x}px, ${y}px)`
    }

    labelStyles.n.transform = projectLabel([0, 0, -4.5]) 
    labelStyles.e.transform = projectLabel([3.5, 0, 0])  
    labelStyles.d.transform = projectLabel([0, -3.0, 0]) 

    gl.useProgram(programInfo.program)
    let rectMat = m4.multiply(viewProj, world)
    rectMat = m4.scale(rectMat, [2.0, 0.6, 3.5]) 
    twgl.setBuffersAndAttributes(gl, programInfo, cubeBufferInfo)
    twgl.setUniforms(programInfo, { u_matrix: rectMat, u_color: [0.1, 0.14, 0.49, 1.0], u_isBody: true })
    twgl.drawBufferInfo(gl, cubeBufferInfo)

    const drawArrow = (direction, length, color) => {
      let mat = m4.multiply(viewProj, world)
      if (direction === 'N') mat = m4.axisRotate(mat, [0, 1, 0], Math.PI / 2)
      if (direction === 'D') mat = m4.axisRotate(mat, [0, 0, 1], -Math.PI / 2)
      twgl.setBuffersAndAttributes(gl, programInfo, lineBufferInfo)
      twgl.setUniforms(programInfo, { u_matrix: m4.scale(mat, [length, 1, 1]), u_color: color, u_isBody: false })
      twgl.drawBufferInfo(gl, lineBufferInfo, gl.LINES)
      let cMat = m4.translate(mat, [length, 0, 0])
      twgl.setBuffersAndAttributes(gl, programInfo, coneBufferInfo)
      twgl.setUniforms(programInfo, { u_matrix: m4.axisRotate(cMat, [0, 0, 1], -Math.PI / 2), u_color: color, u_isBody: false })
      twgl.drawBufferInfo(gl, coneBufferInfo)
    }

    drawArrow('N', 4.0, [1, 0, 0, 1])
    drawArrow(null, 3.0, [0, 0, 1, 1])
    drawArrow('D', 2.5, [0, 0.8, 0, 1])
    
    requestAnimationFrame(render)
  }
  requestAnimationFrame(render)
})

const vs = `attribute vec4 position; uniform mat4 u_matrix; varying float v_y; void main() { gl_Position = u_matrix * position; v_y = position.y; }`;
const fs = `precision mediump float; uniform vec4 u_color; uniform bool u_isBody; varying float v_y; void main() { if (u_isBody) { float highlight = (v_y > 0.0) ? 0.15 : 0.0; gl_FragColor = vec4(u_color.rgb + highlight, u_color.a); } else { gl_FragColor = u_color; } }`;
</script>

<style scoped>
.canvas-wrapper {
  position: relative;
  width: 100%;
  height: 400px;
  background: #fdfdfd;
  border-radius: 12px;
  overflow: hidden;
}

@media (max-width: 768px) {
  .canvas-wrapper { height: 300px; }
}

.hud-overlay {
  position: absolute;
  top: 10px;
  left: 10px;
  z-index: 10;
  background: rgba(255, 255, 255, 0.8);
  padding: 6px 10px;
  border-radius: 4px;
  border-left: 3px solid #485fc7;
  pointer-events: none;
}

.hud-label { color: #666; font-family: monospace; margin-right: 4px; }
.hud-line { font-size: 0.85rem; font-weight: 900; line-height: 1.2; }

.label-3d {
  position: absolute;
  top: 0; left: 0;
  z-index: 5;
  font-weight: 900;
  font-size: 1rem;
  pointer-events: none;
  text-shadow: 1px 1px 2px white;
  transform: translate(-100px, -100px); /* Initial hide */
}

.n-label { color: #ff3860; }
.e-label { color: #485fc7; }
.d-label { color: #48c78e; }

.attitude-canvas { width: 100%; height: 100%; display: block; }
.shadow-card { border-radius: 16px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); border: 1px solid #efefef; }

@media (max-width: 768px) {
  .is-justify-content-center-mobile {
    flex-direction: column; /* Force the vertical stack */
  }
  .button.is-small {
    margin-top: 5px; /* Clean gap between stacked elements */
  }
}
</style>
