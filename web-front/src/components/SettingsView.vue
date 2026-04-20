<template>
  <div class="settings-view px-2 pb-6">
    
    <!-- 1. PERFORMANCE (STAYS LIVE) -->
    <div class="column is-12 p-0">
      <div class="card shadow-card has-background-white mb-5">
        <div class="card-content px-5 py-5">
          <p class="heading has-text-weight-bold has-text-black mb-5">PERFORMANCE & THROTTLING</p>
          <div class="columns is-variable is-8">
            <div class="column is-6">
              <div class="is-flex is-align-items-center mb-2" style="gap: 0.75rem;">
                <span class="is-size-7 has-text-grey-light uppercase tracking-wide">Packet Drop</span>
                <span class="is-family-monospace is-size-5 has-text-weight-bold has-text-link">{{ packetDrop }}ms</span>
              </div>
              <input class="slider is-fullwidth is-link m-0" type="range" min="0" max="500" step="10" v-model.number="packetDrop">
            </div>
            <div class="column is-6">
              <div class="is-flex is-align-items-center mb-2" style="gap: 0.75rem;">
                <span class="is-size-7 has-text-grey-light uppercase tracking-wide">Display Sync</span>
                <span class="is-family-monospace is-size-5 has-text-weight-bold has-text-link">{{ displaySync }}ms</span>
              </div>
              <input class="slider is-fullwidth is-link m-0" type="range" min="0" max="1000" step="50" v-model.number="displaySync">
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 2. IMU ENGINE (LOCAL DRAFT) -->
    <div class="column is-12 mt-5 p-0">
      <div class="card shadow-card has-background-white mb-5">
        <div class="card-content px-5 py-5">
          <p class="heading has-text-weight-bold has-text-black mb-5">IMU ENGINE CONFIG</p>
          <div class="columns is-multiline">
            <div class="column is-6 py-1">
              <label class="label is-size-7 has-text-grey-light uppercase">AHRS Algorithm</label>
              <div class="field has-addons">
                <p class="control is-expanded">
                  <button class="button is-small is-fullwidth is-family-monospace" 
                    :class="localData.imu.ahrs_mode === 'Mahony' ? 'is-link' : 'is-light'" 
                    @click="localData.imu.ahrs_mode = 'Mahony'">MAHONY</button>
                </p>
                <p class="control is-expanded">
                  <button class="button is-small is-fullwidth is-family-monospace" 
                    :class="localData.imu.ahrs_mode === 'Madgwick' ? 'is-link' : 'is-light'" 
                    @click="localData.imu.ahrs_mode = 'Madgwick'">MADGWICK</button>
                </p>
              </div>
            </div>
            <div class="column is-6 py-1">
              <label class="label is-size-7 has-text-grey-light uppercase">Declination (°)</label>
              <input class="input is-small is-family-monospace" type="number" step="0.001" 
                     v-model.number="localData.imu.mag_declination"
                     :class="{'is-danger': isNotNumber(localData.imu.mag_declination)}"
                     @keypress="blockNonNumeric">
            </div>

            <!-- DYNAMIC PARAMS -->
            <template v-if="localData.imu.ahrs_mode === 'Madgwick'">
              <div class="column is-12 py-1">
                <div class="is-flex is-align-items-center mb-1" style="gap: 0.75rem;">
                  <span class="is-size-7 has-text-grey-light uppercase">Madgwick Beta</span>
                  <span class="is-family-monospace is-size-6 has-text-weight-bold has-text-link">
                    {{ localData.imu.beta?.toFixed(3) }}
                  </span>
                </div>
                <input class="slider is-fullwidth is-link is-small m-0" type="range" min="0" max="3" step="0.001"
                       v-model.number="localData.imu.beta">
              </div>
            </template>

            <template v-if="localData.imu.ahrs_mode === 'Mahony'">
              <div class="column is-6 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">twoKp</label>
                <input class="input is-small is-family-monospace" type="number" step="0.001" 
                       v-model.number="localData.imu.twoKp"
                       :class="{'is-danger': isNotNumber(localData.imu.twoKp) || localData.imu.twoKp < 0}"
                       @keypress="blockNonNumeric">
              </div>
              <div class="column is-6 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">twoKi</label>
                <input class="input is-small is-family-monospace" type="number" step="0.001" 
                       v-model.number="localData.imu.twoKi"
                       :class="{'is-danger': isNotNumber(localData.imu.twoKi) || localData.imu.twoKi < 0}"
                       @keypress="blockNonNumeric">
              </div>
            </template>
          </div>
          <!-- ACTION BAR: LIVE PARAMETER PATCH -->
          <div class="mt-5 pt-4 border-top-dashed">
            <div class="is-flex is-align-items-center is-justify-content-space-between mb-3">
              <div class="is-flex is-align-items-center">
                <Icon :icon="tuneIcon" class="has-text-info mr-2" style="font-size: 1rem;" />
                <span class="is-size-7 has-text-grey-dark has-text-weight-bold uppercase tracking-wide">
                  Live-patch AHRS fusion logic
                </span>
              </div>
            </div>

            <button class="button is-fullwidth btn-engineering-save"
                    :disabled="isImuInvalid"
                    @click="updateEngineOnly" >
              <span class="is-flex is-align-items-center">
                <Icon :icon="autoFixIcon" class="mr-2" style="font-size: 1.1rem;" />
                APPLY 
              </span>
            </button>
          </div>

        </div>
      </div>
    </div>

    <!-- 3. WIFI (LOCAL DRAFT) -->
    <div class="column is-12 mt-5 p-0">
      <div class="card shadow-card has-background-white">
        <div class="card-content px-5 py-5">
          <p class="heading has-text-weight-bold has-text-black mb-5">WIFI INTERFACES</p>
          <div class="is-flex is-align-items-center is-justify-content-space-between mb-5 pb-4 border-bottom">
            <span class="is-size-7 has-text-grey-light uppercase tracking-wide">Station Mode Uplink</span>
            <label class="checkbox">
              <input type="checkbox" v-model="localData.wifi.sta_enabled">
              <span class="ml-2 is-size-7 has-text-weight-bold uppercase">Enable</span>
            </label>
          </div>

          <div v-if="localData.wifi.sta_enabled" class="mb-5 pb-5 border-bottom">
            <div class="columns is-multiline">
              <div class="column is-6 py-1"><label class="label is-size-7 has-text-grey-light uppercase">SSID</label>
                <input class="input is-small is-family-monospace" 
                       v-model="localData.wifi.sta_ssid"
                       maxlength="31"
                       :class="{'is-danger': !localData.wifi.sta_ssid || localData.wifi.sta_ssid.length > 31}">
              </div>
              <div class="column is-6 py-1"><label class="label is-size-7 has-text-grey-light uppercase">Password</label>
                <input class="input is-small is-family-monospace" type="password" 
                       v-model="localData.wifi.sta_password"
                       maxlength="63"
                       :class="{'is-danger': localData.wifi.sta_password && localData.wifi.sta_password.length > 63}">
              </div>
            </div>
          </div>
          <!-- ACCESS POINT: THE LOCAL DOOR -->
          <div>
            <p class="is-size-7 has-text-grey-darker has-text-weight-bold mb-3 uppercase tracking-wide">Access Point (Local)</p>
            <div class="columns is-multiline">
              <div class="column is-4 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">AP SSID</label>
                <input class="input is-small is-family-monospace" 
                       v-model="localData.wifi.ap_ssid"
                       maxlength="31"
                       :class="{'is-danger': !localData.wifi.ap_ssid || localData.wifi.ap_ssid.length > 31}">
              </div>
              <div class="column is-4 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">AP Password</label>
                <input class="input is-small is-family-monospace" type="password" 
                       v-model="localData.wifi.ap_password"
                       maxlength="63"
                       :class="{'is-danger': localData.wifi.ap_password && localData.wifi.ap_password.length > 63}">
              </div>
              <div class="column is-4 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">Channel</label>
                <div class="select is-small is-fullwidth">
                  <select v-model.number="localData.wifi.channel" class="is-family-monospace">
                    <option v-for="n in channels" :key="n" :value="n" v-once>CH {{ n }}</option>
                  </select>
                </div>
              </div>

              <div class="column is-6 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">AP IP Address</label>
                <input class="input is-small is-family-monospace" 
                       v-model="localData.wifi.ap_ip" placeholder="192.168.4.1"
                       :class="{'is-danger': !isValidIp(localData.wifi.ap_ip)}">
              </div>
              <div class="column is-6 py-1">
                <label class="label is-size-7 has-text-grey-light uppercase">Subnet Mask</label>
                <input class="input is-small is-family-monospace" 
                       v-model="localData.wifi.ap_mask" placeholder="255.255.255.0"
                       :class="{'is-danger': !isValidIp(localData.wifi.ap_mask)}">
              </div>
            </div>
          </div>
          <div class="mt-6 pt-5 border-top-dashed">
            <div class="is-flex is-align-items-center is-justify-content-space-between mb-4">
              <div class="is-flex is-align-items-center">
                <Icon :icon="alertCircleOutline" class="has-text-warning mr-2" style="font-size: 1.2rem;" />
                <span class="is-size-7 has-text-grey-dark has-text-weight-semibold uppercase tracking-wide">
                  Hardware restart required to apply network changes
                </span>
              </div>
            </div>
            
            <button 
              class="button is-fullwidth btn-engineering-save"
              :disabled="isWifiInvalid"
              @click="saveWiFiOnly"
            >
              <span class="is-flex is-align-items-center">
                <Icon :icon="contentSaveOutline" class="mr-2" style="font-size: 1.1rem;" />
                Apply & REBOOT
              </span>
            </button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue'
import { useIMUStore } from '../store/imuStore'
import { Icon } from '@iconify/vue'
import alertCircleOutline from '@iconify-icons/mdi/alert-circle-outline'
import contentSaveOutline from '@iconify-icons/mdi/content-save-outline'
import tuneIcon from '@iconify-icons/mdi/tune-vertical'
import autoFixIcon from '@iconify-icons/mdi/auto-fix'

const channels = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]

const imuStore = useIMUStore()
const { state } = imuStore

const isValidIp = (ip) => {
  if (!ip || typeof ip !== 'string') return false
  const parts = ip.trim().split('.')
  if (parts.length !== 4) return false
  return parts.every(p => {
    const n = parseInt(p, 10)
    // Check if it's purely numeric and within range
    return /^[0-9]{1,3}$/.test(p) && n >= 0 && n <= 255
  })
}

const isNotNumber = (val) => {
  if (val === null || val === undefined || val === '') return true
  return isNaN(Number(val))
}

const blockNonNumeric = (e) => {
  const char = e.key;
  const isDigit = /[0-9]/.test(char);
  const isDot = char === '.';
  const isMinus = char === '-';

  if (!isDigit && !isDot && !isMinus) {
    if (char.length === 1) e.preventDefault();
    return;
  }

  const val = e.target.value;
  if (isDot && val.includes('.')) {
    e.preventDefault();
  }
  if (isMinus && val.includes('-')) {
    e.preventDefault();
  }
};

const isImuInvalid = computed(() => {
  const imu = localData.value.imu
  if (!imu) return true
  if (isNotNumber(imu.mag_declination)) return true
  if (imu.ahrs_mode === 'Mahony') {
    if (isNotNumber(imu.twoKp) || imu.twoKp < 0) return true
    if (isNotNumber(imu.twoKi) || imu.twoKi < 0) return true
  }
  return false
})

const isWifiInvalid = computed(() => {
  const wifi = localData.value.wifi
  if (!wifi) return true
  
  // SSID max 31 (char[32] with null), Password max 63 (char[64] with null)
  if (!wifi.ap_ssid || wifi.ap_ssid.length > 31) return true
  if (wifi.ap_password && wifi.ap_password.length > 63) return true
  
  if (!isValidIp(wifi.ap_ip)) return true
  if (!isValidIp(wifi.ap_mask)) return true
  
  if (wifi.sta_enabled) {
    if (!wifi.sta_ssid || wifi.sta_ssid.length > 31) return true
    if (wifi.sta_password && wifi.sta_password.length > 63) return true
  }
  return false
})

// --- 1. PERFORMANCE (LIVE / NO SANDBOX) ---
// These update the store and LocalStorage IMMEDIATELY
const packetDrop = computed({
  get: () => state.packetDropMs,
  set: (val) => imuStore.setPacketDrop(val)
})

const displaySync = computed({
  get: () => state.displayUpdateMs,
  set: (val) => imuStore.setDisplayRate(val)
})

// --- 2. HARDWARE (SANDBOXED / DRAFT) ---
const localData = ref({
  wifi: {},
  imu: {},
})

const syncFromStore = () => {
  localData.value = JSON.parse(JSON.stringify({
    wifi: state.settings.wifi,
    imu: state.settings.imu,
  }))
}

// Reset hardware draft on entry, but LEAVE performance settings alone
watch(() => state.currentView, (newView) => {
  if (newView === 'settings') syncFromStore()
})

onMounted(syncFromStore)

// --- 1. THE LIVE PATCH (IMU Engine) ---
const updateEngineOnly = async () => {
  // 1. Prepare the payload (Hardware Sandbox -> POST)
  const payload = {
    ahrs_mode: localData.value.imu.ahrs_mode,
    beta: localData.value.imu.beta,
    twoKp: localData.value.imu.twoKp,
    twoKi: localData.value.imu.twoKi,
    mag_declination: localData.value.imu.mag_declination // Included here
  }

  try {
    const res = await fetch('/api/settings/imu', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    })

    if (res.ok) {
      // 2. THE ECHO: ESP32 confirms what it saved
      const confirmed = await res.json()
      
      // 3. SURGICAL SYNC: Split the data back into the correct Store blocks
      // Update AHRS params
      imuStore.updateIMUEngine({
        ahrs_mode: confirmed.ahrs_mode,
        beta: confirmed.beta,
        twoKp: confirmed.twoKp,
        twoKi: confirmed.twoKi,
        mag_declination: confirmed.mag_declination 
      })
      alert("IMU Engine & Declination Updated")
    }
  } catch (e) {
    alert("Update Failed: " + e.message)
  }
}

const saveWiFiOnly = async () => {
  if (!confirm("Reboot hardware to apply network changes?")) return
  
  try {
    const res = await fetch('/api/settings/wifi', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(localData.value.wifi)
    })

    if (res.ok) {
      const confirmedWiFi = await res.json()
      // Sync store before the connection drops for the reboot
      imuStore.updateWifiSettings(confirmedWiFi)
      alert("WiFi Updated. Rebooting...")
    }
  } catch (e) { alert("Save Failed: " + e.message) }
}

</script>

<style scoped>
.shadow-card {
  box-shadow: 0 2px 15px rgba(0,0,0,0.05);
  border-radius: 8px;
}

.is-family-monospace {
  font-family: 'Courier New', Courier, monospace;
}

.uppercase {
  text-transform: uppercase; letter-spacing: 0.1em;
}

.slider {
  cursor: pointer;
  height: 8px;
}

.border-bottom {
  border-bottom: 1px solid #f0f0f0;
}

.input.is-small, .select.is-small select {
  background-color: #f7f9fa;
  border: 1px solid #eee;
  transition: all 0.2s ease;
  color: #2c3e50;
}

.input.is-small:focus, .select.is-small select:focus {
  background-color: #fff;
  border-color: #485fc7;
  box-shadow: none;
}

.label.is-size-7 {
  margin-bottom: 0.25rem !important;
  color: #7a7a7a !important;
  letter-spacing: 0.05em;
}

/* Dashed separator for engineering feel */
.border-top-dashed {
  border-top: 1px dashed #e0e0e0;
}

/* The Professional 'No-Slop' Button */
.btn-engineering-save {
  background-color: #2c3e50; /* Deep Slate */
  color: #ffffff;
  border: none;
  border-radius: 6px;
  height: 48px;
  text-transform: uppercase;
  letter-spacing: 0.15em;
  font-weight: 700;
  font-size: 0.75rem;
  box-shadow: 0 4px 0 #1a252f; /* Fixed bottom shadow for depth */
  transition: all 0.1s ease;
}

.btn-engineering-save:hover {
  background-color: #34495e;
  color: white;
  transform: translateY(1px);
  box-shadow: 0 3px 0 #1a252f;
}

.btn-engineering-save:active {
  transform: translateY(4px);
  box-shadow: none;
}

/* Warning text styling */
.uppercase { text-transform: uppercase; }
.tracking-wide { letter-spacing: 0.08em; }
</style>
