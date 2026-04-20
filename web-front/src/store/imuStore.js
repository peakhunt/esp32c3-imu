import { reactive, readonly, watch } from 'vue'

const isMobile = /Android|iPhone|iPad|iPod/i.test(navigator.userAgent) || 
                 (navigator.maxTouchPoints > 0 && window.innerWidth <= 1024)

const savedPacketDrop = localStorage.getItem('imu_packetDropMs')
const savedDisplaySync = localStorage.getItem('imu_displayUpdateMs')

const state = reactive({
  currentView: 'dashboard',
  packetDropMs: savedPacketDrop !== null ? parseInt(savedPacketDrop) : (isMobile ? 100 : 0),
  displayUpdateMs: savedDisplaySync !== null ? parseInt(savedDisplaySync) : (isMobile ? 0 : 0),

  roll: 0.0, pitch: 0.0, yaw: 0.0,
  gyro: { x: 0, y: 0, z: 0 },
  accel: { x: 0, y: 0, z: 0 },
  mag: { x: 0, y: 0, z: 0 },
  cpuUsage: 0,
  samplingRate: 0,
  sensorTrx: 0,
  sensorTrxFailed: 0,
  isCalibrating: false,
  stats: { packetsReceived: 0, connected: false },
  settings: {
    calibration: {
      accel_off: [0,0,0], accel_scale: [1,1,1],
      gyro_off: [0,0,0], mag_bias: [0,0,0],
      mag_scale: [1,1,1]
    },
    imu: {
      ahrs_mode: 'Madgwick',
      beta: 0.1,
      twoKp: 2.0,
      twoKi: 0.005,
      mag_declination: 0.0
    },
    wifi: {
      sta_enabled: true, sta_ssid: 'apid', sta_password: '',
      ap_ssid: 'imu', ap_password: '1234',
      ap_ip: '192.168.4.1', ap_mask: '255.255.255.0', channel: 1
    }
  }
})

let bufIMU = { r: 0, p: 0, y: 0, g: null, a: null, m: null }
let bufStats = { cpu: 0, rate: 0, tx: 0, fail: 0 }
let lastUiSyncTime = 0

const syncToState = () => {
  state.roll = bufIMU.r; state.pitch = bufIMU.p; state.yaw = bufIMU.y
  if (bufIMU.g) state.gyro = bufIMU.g
  if (bufIMU.a) state.accel = bufIMU.a
  if (bufIMU.m) state.mag = bufIMU.m
  state.cpuUsage = bufStats.cpu
  state.samplingRate = bufStats.rate
  state.sensorTrx = bufStats.tx
  state.sensorTrxFailed = bufStats.fail
}

const updateIMU = (r, p, y, g, a, m) => {
  bufIMU = { r, p, y, g, a, m }
  state.stats.packetsReceived++
  const now = performance.now()
  if (now - lastUiSyncTime >= state.displayUpdateMs) {
    syncToState(); lastUiSyncTime = now
  }
}

const updateSystemStats = (cpu, rate, tx, fail) => {
  bufStats = { cpu, rate, tx: Number(tx), fail: Number(fail) }
  const now = performance.now()
  if (now - lastUiSyncTime >= state.displayUpdateMs) {
    syncToState(); lastUiSyncTime = now
  }
}

watch(() => state.packetDropMs, (v) => localStorage.setItem('imu_packetDropMs', v))
watch(() => state.displayUpdateMs, (v) => localStorage.setItem('imu_displayUpdateMs', v))

export const useIMUStore = () => ({
  state: readonly(state),
  setView: (view) => { state.currentView = view },
  updateIMU,
  updateSystemStats,
  setPacketDrop: (ms) => { state.packetDropMs = ms },
  setDisplayRate: (ms) => { state.displayUpdateMs = ms },
  updateWifiSettings: (u) => Object.assign(state.settings.wifi, u),
  updateIMUEngine: (u) => Object.assign(state.settings.imu, u),
  updateCalibration: (u) => Object.assign(state.settings.calibration, u),
  setConnected: (s) => { state.stats.connected = s },
  setCalibrating: (s) => { state.isCalibrating = s },
  setDefaults: () => {
    state.packetDropMs = isMobile ? 100 : 0
    state.displayUpdateMs = 0
  },
  fetchSettings: async () => {
    try {
      const res = await fetch('/api/settings')
      const data = await res.json()
      if (data.calibration) Object.assign(state.settings.calibration, data.calibration)
      if (data.wifi) Object.assign(state.settings.wifi, data.wifi)
      if (data.imu) Object.assign(state.settings.imu, data.imu)
    } catch (e) { console.error(e) }
  }
})
