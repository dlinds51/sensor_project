<template>
  <div class="servo-slider">
    <div class="servo-caption">Servo command: {{ signedAngle }}° (0 = center)</div>
    <input
      type="range" min="-90" max="90" step="1" v-model.number="angle"
      aria-label="Servo angle minus ninety through plus ninety"
      @change="sendAngle"
    />
    <div class="servo-ends"><span>-90°</span><span>0°</span><span>+90°</span></div>
    <div class="servo-buttons">
      <button type="button" @click="go(-90)">-90°</button>
      <button type="button" @click="go(0)">Zero</button>
      <button type="button" @click="go(90)">+90°</button>
    </div>
    <div class="servo-note">Slider sends on RELEASE (not each mouse move) to avoid command flooding.</div>
    <div v-if="error" class="servo-error">{{ error }}</div>
  </div>
</template>
<script>
import { Widget } from '@openc3/vue-common/widgets'
import { OpenC3Api } from '@openc3/js-common/services'
export default {
  mixins: [Widget],
  data() { return { angle: 0, error: '' } },
  computed: { signedAngle() { return this.angle > 0 ? '+' + this.angle : String(this.angle) } },
  methods: {
    go(v) { this.angle = v; this.sendAngle() },
    async sendAngle() {
      try {
        const deg = Number(this.angle)
        if (!Number.isInteger(deg) || deg < -90 || deg > 90) throw Error('Invalid angle')
        const api = new OpenC3Api()
        await api.cmd('MKR_RTD', 'SET_SERVO', { ANGLE_ABS: deg + 90 })
        this.error = ''
      } catch (error) {
        this.error = 'Command failed: ' + (error?.message || String(error))
      }
    },
  },
}
</script>
<style scoped>
.servo-slider { padding: 14px; min-width: 340px; max-width: 650px; }
.servo-caption { margin-bottom: 10px; font-weight: 600; text-align: center; }
.servo-slider input[type=range] { width: 100%; }
.servo-ends, .servo-buttons { display: flex; justify-content: space-between; gap: 8px; }
.servo-buttons { margin-top: 14px; }
.servo-buttons button { padding: 8px 14px; border: 1px solid #888; border-radius: 6px; }
.servo-note { font-size: 0.8em; margin-top: 12px; }
.servo-error { color: #c33; margin-top: 6px; }
</style>
