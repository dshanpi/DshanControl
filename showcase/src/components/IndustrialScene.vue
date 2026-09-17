<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as THREE from 'three'
import { useSimulationStore } from '../stores/simulation'

const props = withDefaults(defineProps<{ variant?: 'hero' | 'topology' | 'protocol'; interactive?: boolean }>(), { variant: 'topology', interactive: true })
const emit = defineEmits<{ ready: []; select: [id: string] }>()
const host = ref<HTMLDivElement | null>(null)
const failed = ref(false)
const showPoster = ref(true)
const posters = {
  hero: new URL('../../design/references/01-solution-home.png', import.meta.url).href,
  topology: new URL('../../design/references/02-interactive-topology.png', import.meta.url).href,
}
const posterSrc = computed(() => props.variant === 'protocol' ? '' : posters[props.variant])
const store = useSimulationStore()
let renderer: THREE.WebGLRenderer | undefined
let scene: THREE.Scene
let camera: THREE.PerspectiveCamera
let frame = 0
let resize: ResizeObserver | undefined
const pickables: THREE.Object3D[] = []
const deviceGroups = new Map<string, THREE.Group>()
const pointer = new THREE.Vector2()
const raycaster = new THREE.Raycaster()
let dragging = false
let lastX = 0
let targetRotation = -0.08

const metal = new THREE.MeshStandardMaterial({ color: 0xdde6ef, metalness: .55, roughness: .3 })
const white = new THREE.MeshStandardMaterial({ color: 0xf6f9fc, metalness: .25, roughness: .34 })
const dark = new THREE.MeshStandardMaterial({ color: 0x15273b, metalness: .5, roughness: .34 })
const blue = new THREE.MeshStandardMaterial({ color: 0x1172f4, emissive: 0x063c8a, emissiveIntensity: .28, metalness: .32, roughness: .28 })
const rubber = new THREE.MeshStandardMaterial({ color: 0x24313d, roughness: .7 })
const glass = new THREE.MeshStandardMaterial({ color: 0x9dd8ff, transparent: true, opacity: .48, metalness: .05, roughness: .1 })

function box(w: number, h: number, d: number, mat = white, x = 0, y = 0, z = 0) {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat)
  mesh.position.set(x, y, z)
  mesh.castShadow = true
  mesh.receiveShadow = true
  return mesh
}

function cylinder(r: number, h: number, mat = metal, x = 0, y = 0, z = 0, rotateZ = 0) {
  const mesh = new THREE.Mesh(new THREE.CylinderGeometry(r, r, h, 24), mat)
  mesh.position.set(x, y, z)
  mesh.rotation.z = rotateZ
  mesh.castShadow = true
  return mesh
}

function register(group: THREE.Group, id: string) {
  group.userData.deviceId = id
  group.traverse(child => { child.userData.deviceId = id; if ((child as THREE.Mesh).isMesh) pickables.push(child) })
  deviceGroups.set(id, group)
  scene.add(group)
}

function addConveyor() {
  const group = new THREE.Group()
  group.position.set(-3.15, .55, .5)
  group.add(box(6.3, .22, 1.35, dark))
  for (let i = -7; i <= 7; i++) group.add(cylinder(.08, 1.2, metal, i * .4, .15, 0, Math.PI / 2))
  for (const x of [-2.8, -1.2, .4, 2, 2.8]) {
    group.add(box(.12, .8, .12, metal, x, -.45, .48)); group.add(box(.12, .8, .12, metal, x, -.45, -.48))
  }
  for (const x of [-2.4, -.9, .7, 2.2]) group.add(box(.52, .52, .52, new THREE.MeshStandardMaterial({ color: 0xcaa06c }), x, .48, 0))
  scene.add(group)
}

function addGateway() {
  const group = new THREE.Group()
  group.position.set(1.4, 1.02, .45)
  group.add(box(1.5, 1.55, 1.25, dark))
  group.add(box(1.56, .08, 1.15, new THREE.MeshStandardMaterial({ color: 0x263a4a }), 0, .42, 0))
  for (let i = 0; i < 5; i++) group.add(box(.18, .3, .04, blue, -.5 + i * .25, -.1, .65))
  for (let i = 0; i < 8; i++) group.add(box(.05, .04, .6, rubber, -.5 + i * .14, .79, 0))
  register(group, 'gateway-01')
}

function addMotor() {
  const group = new THREE.Group()
  group.position.set(-5.5, .85, -.8)
  const body = cylinder(.58, 1.15, new THREE.MeshStandardMaterial({ color: 0x6f8ea6, metalness: .72, roughness: .36 }), 0, 0, 0, Math.PI / 2)
  group.add(body)
  for (let i = -4; i <= 4; i++) group.add(box(.05, 1.26, .85, metal, i * .11, 0, 0))
  group.add(cylinder(.17, .8, dark, .85, 0, 0, Math.PI / 2)); group.add(box(1.25, .15, .9, dark, 0, -.62, 0))
  register(group, 'motor-02')
}

function addMeter() {
  const group = new THREE.Group()
  group.position.set(3.3, 1.05, -1.85)
  group.add(box(.9, 1.35, .42, white)); group.add(box(.68, .46, .03, glass, 0, .18, .23)); group.add(box(.58, .1, .03, dark, 0, .56, .23))
  for (let i = 0; i < 4; i++) group.add(cylinder(.07, .1, metal, -.27 + i * .18, -.65, 0, 0))
  register(group, 'meter-01')
}

function addSensor() {
  const group = new THREE.Group()
  group.position.set(-1.5, .8, 1.5)
  group.add(cylinder(.16, .75, metal)); group.add(cylinder(.3, .22, white, 0, .42, 0)); group.add(cylinder(.07, .68, metal, 0, -.68, 0))
  const ring = new THREE.Mesh(new THREE.TorusGeometry(.22, .045, 12, 30), blue); ring.rotation.x = Math.PI / 2; ring.position.y = .25; group.add(ring)
  register(group, 'temp-03')
}

function addRobot() {
  const group = new THREE.Group(); group.position.set(-1.2, 1.05, -1.4)
  group.add(cylinder(.52, .3, white)); const arm1 = box(.42, 1.5, .42, white, 0, .9, 0); arm1.rotation.z = -.42; group.add(arm1)
  group.add(cylinder(.36, .3, blue, .32, 1.58, 0, 0)); const arm2 = box(.35, 1.25, .35, white, .78, 2.05, 0); arm2.rotation.z = -.65; group.add(arm2)
  group.add(cylinder(.23, .3, dark, 1.18, 2.54, 0, 0)); scene.add(group)
}

function addMonitor() {
  const group = new THREE.Group(); group.position.set(5.1, 1.65, .15)
  group.add(box(2.2, 1.35, .12, dark)); group.add(box(1.96, 1.12, .03, new THREE.MeshStandardMaterial({ color: 0xcfe8ff, emissive: 0x7ab8ff, emissiveIntensity: .22 }), 0, 0, -.08))
  group.add(cylinder(.12, .85, metal, 0, -.98, 0)); group.add(box(1.15, .12, .55, metal, 0, -1.38, 0)); register(group, 'display-01')
}

function addCable(points: THREE.Vector3[]) {
  const curve = new THREE.CatmullRomCurve3(points)
  const tube = new THREE.Mesh(new THREE.TubeGeometry(curve, 48, .035, 8, false), new THREE.MeshStandardMaterial({ color: 0x1577ff, emissive: 0x075edc, emissiveIntensity: .5 }))
  scene.add(tube)
  for (let i = .08; i < 1; i += .1) { const point = curve.getPoint(i); const marker = new THREE.Mesh(new THREE.SphereGeometry(.07, 10, 10), blue); marker.position.copy(point); scene.add(marker) }
}

function buildScene() {
  scene = new THREE.Scene()
  scene.background = new THREE.Color(0xf2f7fd)
  scene.fog = new THREE.Fog(0xf2f7fd, 16, 27)
  camera = new THREE.PerspectiveCamera(props.variant === 'hero' ? 33 : 38, 1, .1, 100)
  camera.position.set(props.variant === 'hero' ? 10 : 9, props.variant === 'hero' ? 8 : 7, props.variant === 'hero' ? 11 : 12)
  camera.lookAt(0, .6, 0)
  scene.add(new THREE.HemisphereLight(0xffffff, 0x8ca7c6, 2.4))
  const key = new THREE.DirectionalLight(0xffffff, 3.4); key.position.set(-5, 12, 7); key.castShadow = true; key.shadow.mapSize.set(2048, 2048); scene.add(key)
  const floor = box(15, .3, 8.5, new THREE.MeshStandardMaterial({ color: 0xe9f0f7, metalness: .1, roughness: .5 }), 0, -.26, 0); scene.add(floor)
  const grid = new THREE.GridHelper(14, 30, 0xcad9ea, 0xdce6f2); grid.position.y = -.1; (grid.material as THREE.Material).opacity = .42; (grid.material as THREE.Material).transparent = true; scene.add(grid)
  addConveyor(); addGateway(); addMotor(); addMeter(); addSensor(); addRobot(); addMonitor()
  addCable([new THREE.Vector3(-5,.2,-.8), new THREE.Vector3(-3,0,-1.2), new THREE.Vector3(-1,.1,.2), new THREE.Vector3(1.4,.15,.45)])
  addCable([new THREE.Vector3(-1.5,.12,1.5), new THREE.Vector3(0,.05,1.8), new THREE.Vector3(.6,.15,1), new THREE.Vector3(1.4,.3,.45)])
  addCable([new THREE.Vector3(1.4,.25,.45), new THREE.Vector3(3,.15,.3), new THREE.Vector3(4,.2,.1), new THREE.Vector3(5.1,.45,.15)])
}

function render() {
  frame = requestAnimationFrame(render)
  scene.rotation.y += (targetRotation - scene.rotation.y) * .06
  const motor = deviceGroups.get('motor-02')
  if (motor && store.running) motor.rotation.x += .004 * store.speed
  renderer?.render(scene, camera)
}

function resizeCanvas() {
  if (!host.value || !renderer) return
  const { width, height } = host.value.getBoundingClientRect()
  renderer.setSize(width, height, false); renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5)); camera.aspect = width / height; camera.updateProjectionMatrix()
}

function pointerPosition(event: PointerEvent) {
  const rect = host.value!.getBoundingClientRect(); pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1; pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1
}

function onPointerDown(event: PointerEvent) { showPoster.value = false; dragging = true; lastX = event.clientX; host.value?.setPointerCapture(event.pointerId) }
function onPointerMove(event: PointerEvent) {
  if (!host.value) return
  if (dragging && props.interactive) { targetRotation += (event.clientX - lastX) * .003; targetRotation = Math.max(-.36, Math.min(.28, targetRotation)); lastX = event.clientX; return }
  pointerPosition(event); raycaster.setFromCamera(pointer, camera); host.value.style.cursor = raycaster.intersectObjects(pickables, false).length ? 'pointer' : 'grab'
}
function onPointerUp(event: PointerEvent) {
  if (!host.value) return
  const moved = Math.abs(event.clientX - lastX); dragging = false
  if (moved > 4) return
  pointerPosition(event); raycaster.setFromCamera(pointer, camera); const hit = raycaster.intersectObjects(pickables, false)[0]
  const id = hit?.object.userData.deviceId as string | undefined
  if (id) { store.select(id); emit('select', id) }
}

watch(() => store.selectedDeviceId, id => {
  deviceGroups.forEach((group, key) => group.traverse(child => {
    const mesh = child as THREE.Mesh
    if (!mesh.isMesh) return
    if (!mesh.userData.originalEmissive && (mesh.material as THREE.MeshStandardMaterial).emissive) mesh.userData.originalEmissive = (mesh.material as THREE.MeshStandardMaterial).emissive.getHex()
    const material = mesh.material as THREE.MeshStandardMaterial
    if (material.emissive) material.emissive.setHex(key === id ? 0x105cc4 : mesh.userData.originalEmissive || 0x000000)
  }))
})

onMounted(() => {
  if (!host.value) return
  try {
    buildScene()
    renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' })
  } catch {
    failed.value = true
    emit('ready')
    return
  }
  renderer.outputColorSpace = THREE.SRGBColorSpace; renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.05; renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFSoftShadowMap
  host.value.prepend(renderer.domElement)
  renderer.domElement.addEventListener('webglcontextlost', event => { event.preventDefault(); failed.value = true })
  host.value.addEventListener('pointerdown', onPointerDown); host.value.addEventListener('pointermove', onPointerMove); host.value.addEventListener('pointerup', onPointerUp)
  resize = new ResizeObserver(resizeCanvas); resize.observe(host.value); resizeCanvas(); render(); emit('ready')
})

onBeforeUnmount(() => {
  cancelAnimationFrame(frame); resize?.disconnect(); renderer?.dispose()
  scene?.traverse(object => { const mesh = object as THREE.Mesh; mesh.geometry?.dispose?.(); const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material]; materials.forEach(material => material?.dispose?.()) })
})
</script>

<template>
  <div ref="host" class="industrial-scene" :class="[`scene-${variant}`, { 'has-poster': showPoster && posterSrc }]">
    <img v-if="showPoster && posterSrc" class="reference-poster" :src="posterSrc" alt="" draggable="false">
    <div v-if="failed" class="fallback-scene"><div class="fallback-line"></div><span class="fallback-device motor">电机</span><span class="fallback-device sensor">温度</span><span class="fallback-device gateway">网关</span><span class="fallback-device screen">应用</span><p>兼容模式 · 旋转不可用</p></div>
    <div class="scene-chip sensor-chip"><b>温度传感器</b><span>{{ store.temperature.toFixed(1) }} °C</span><small>Modbus RTU</small></div>
    <div class="scene-label label-gateway">DshanControl<br>工业网关</div>
    <div class="scene-label label-motor">电机</div>
    <div class="scene-label label-meter">电表</div>
    <div class="scene-label label-display">Web 应用</div>
    <span class="concept-tag">概念模型 · 真 3D</span>
  </div>
</template>

<style scoped>
.industrial-scene { position: relative; width: 100%; height: 100%; overflow: hidden; background: radial-gradient(circle at 60% 35%, #fff 0, #eef6ff 58%, #e3effc 100%); }
.industrial-scene :deep(canvas) { position: absolute; inset: 0; width: 100%; height: 100%; outline: 0; }
.reference-poster { position:absolute; z-index:2; width:1672px; height:941px; max-width:none; user-select:none; pointer-events:none; object-fit:fill; }
.scene-hero .reference-poster { left:-702px; top:-67px; }
.scene-topology .reference-poster { left:-72px; top:-268px; clip-path:inset(268px 0 248px 0); }
.has-poster .scene-chip,.has-poster .scene-label,.has-poster .concept-tag { display:none; }
.fallback-scene { position:absolute; inset:0; z-index:2; background:radial-gradient(circle,#fff,#e9f3ff); }.fallback-line { position:absolute; left:15%; right:15%; top:54%; height:4px; background:var(--primary); box-shadow:0 0 0 1px #8ec0ff; }.fallback-device { position:absolute; top:45%; width:70px; height:55px; display:grid; place-items:center; border:1px solid #91bcef; border-radius:8px; background:#fff; color:#214769; font-size:12px; }.fallback-device.motor { left:12%; }.fallback-device.sensor { left:34%; }.fallback-device.gateway { left:56%; background:#16324e; color:#fff; }.fallback-device.screen { right:10%; }.fallback-scene p { position:absolute; right:14px; bottom:8px; color:#7187a2; font-size:11px; }
.scene-chip { position: absolute; z-index: 3; left: 24%; top: 38%; min-width: 128px; padding: 10px 13px; border: 1px solid #82b5ff; border-radius: 10px; background: rgba(255,255,255,.94); box-shadow: 0 8px 20px rgba(25,94,186,.14); pointer-events: none; }
.scene-chip b, .scene-chip span, .scene-chip small { display: block; }
.scene-chip b { color: var(--heading); font-size: 13px; }
.scene-chip span { margin: 3px 0 1px; color: var(--primary); font-size: 16px; font-weight: 750; }
.scene-chip small { color: #55708f; }
.scene-label { position: absolute; z-index: 2; color: #18365f; font-size: 12px; text-align: center; font-weight: 650; pointer-events: none; }
.label-gateway { left: 50%; top: 71%; }.label-motor { left: 9%; top: 66%; }.label-meter { left: 68%; top: 35%; }.label-display { right: 10%; top: 39%; }
.concept-tag { position: absolute; right: 12px; bottom: 10px; z-index: 4; padding: 4px 8px; border-radius: 5px; background: rgba(255,255,255,.78); color: #7890ac; font-size: 10px; pointer-events: none; }
.scene-hero .scene-chip { left: 27%; top: 36%; transform: scale(.88); }
.scene-hero .scene-label { display: none; }
@media (max-width: 760px) { .reference-poster { display:none; }.has-poster .scene-chip,.has-poster .scene-label,.has-poster .concept-tag { display:block; }.has-poster .scene-label { display:block; }.scene-chip { transform: scale(.75); transform-origin: left top; }.scene-label { font-size: 10px; } }
</style>
