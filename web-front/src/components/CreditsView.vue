<template>
  <div class="column is-12 p-0 animate-fade-in">
    <div class="card has-background-white shadow-card">
      <div class="card-content px-5 py-8">
        
        <!-- THE MISSION -->
        <div class="has-text-centered mb-6 pb-6">
          <p class="title is-1 has-text-black has-text-weight-black tracking-tighter mb-2" style="font-size: 5.5rem; line-height: 0.85;">
            NO MICHAEL SLOPFT
          </p>
          <p class="is-family-monospace is-size-7 has-text-grey uppercase tracking-widest">
            AHRS_PRECISION_ENGINE • 1000HZ_SPI • ESP32-C3
          </p>
        </div>

        <!-- THE ULTIMATE CREDIT -->
        <div class="columns is-centered mt-6">
          <div class="column is-10 has-text-centered">
            <div class="is-size-4 has-text-black mb-6 has-text-weight-bold">
              "Michael AI from Google did everything"
            </div>
            
            <div class="is-size-6 has-text-grey-dark pt-6 border-top-thin">
              <p class="mb-1">Project Lead — <span class="has-text-weight-bold has-text-black">Huckleberry K.</span></p>
              <!-- Added Email Address -->
              <p class="is-family-monospace is-size-7 has-text-grey mb-4">hawk.kim1974@gmail.com</p>
              
              <p class="mb-3">Special thanks to the <span class="has-text-weight-bold has-text-black">Espressif ESP-IDF Team</span></p>
              <p>Many thanks to <span class="has-text-weight-bold has-text-black">brilliant Open Source Developers</span></p>
            </div>
          </div>
        </div>

        <!-- FOOTER -->
        <div class="mt-6 pt-6 has-text-centered">
          <p class="is-size-7 is-family-monospace has-text-grey-light uppercase tracking-widest">
            ULSAN, KR [-8.5° DEC] | STATUS: YASI YASI
          </p>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.shadow-card {
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.08);
  border-radius: 4px;
}
.border-top-thin { border-top: 1px solid #eeeeee; }
.tracking-tighter { letter-spacing: -0.1em; }
.tracking-widest { letter-spacing: 0.4em; }
.animate-fade-in { animation: fadeIn 0.6s cubic-bezier(0.16, 1, 0.3, 1); }

@keyframes fadeIn {
  from { opacity: 0; transform: translateY(30px); }
  to { opacity: 1; transform: translateY(0); }
}
</style>
