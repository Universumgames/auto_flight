import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { FLAP_DEPTH } from './config.js';

// All tunable animation limits live here, in one place.
const LIMITS = {
  aileronDeg: 20, // control surface deflection
  elevatorDeg: 20,
  rudderDeg: 20,
  propMaxSpeed: 25, // rad/s at full throttle
};

const deg2rad = (d) => (d * Math.PI) / 180;

function makeHingedFlap(width, height, depth, hingeOffsetX) {
  // hinge is an empty Object3D positioned at the flap's mounting edge (the
  // trailing edge of the wing/stab/fin it's attached to). The flap mesh is
  // offset aft (-Z) by half its depth so it hangs behind the hinge line
  // instead of straddling it - rotating the hinge then swings the flap
  // about its mount point, not its own center.
  const hinge = new THREE.Object3D();
  const flapMaterial = new THREE.MeshStandardMaterial({ color: 0xdadada });
  const flap = new THREE.Mesh(new THREE.BoxGeometry(width, height, depth), flapMaterial);
  flap.position.x = hingeOffsetX;
  flap.position.z = -depth / 2;
  hinge.add(flap);
  return hinge;
}

export function createPlaneScene(canvas) {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x1a1d23);

  const camera = new THREE.PerspectiveCamera(
    50,
    canvas.clientWidth / canvas.clientHeight,
    0.05,
    100
  );
  camera.position.set(1.6, 1.0, 1.6);

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(canvas.clientWidth, canvas.clientHeight, false);

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.target.set(0, 0, 0);
  controls.enableDamping = true;

  scene.add(new THREE.HemisphereLight(0xffffff, 0x33363f, 1.2));
  const sun = new THREE.DirectionalLight(0xffffff, 0.8);
  sun.position.set(2, 3, 1);
  scene.add(sun);

  const grid = new THREE.GridHelper(6, 24, 0x444a55, 0x2b2f38);
  scene.add(grid);

  // --- Plane construction -------------------------------------------------
  const bodyMaterial = new THREE.MeshStandardMaterial({ color: 0xe8503a });
  const accentMaterial = new THREE.MeshStandardMaterial({ color: 0xf0f0f0 });

  const planeGroup = new THREE.Group();
  scene.add(planeGroup);

  // Fuselage: nose points toward +Z.
  const fuselage = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.12, 1.0), bodyMaterial);
  planeGroup.add(fuselage);

  // Nose cone.
  const nose = new THREE.Mesh(new THREE.ConeGeometry(0.08, 0.18, 12), accentMaterial);
  nose.rotation.x = Math.PI / 2;
  nose.position.z = 0.59;
  planeGroup.add(nose);

  // Propeller.
  const propGroup = new THREE.Group();
  propGroup.position.z = 0.68;
  const hub = new THREE.Mesh(
    new THREE.CylinderGeometry(0.015, 0.015, 0.05, 8),
    new THREE.MeshStandardMaterial({ color: 0x222222 })
  );
  hub.rotation.x = Math.PI / 2;
  propGroup.add(hub);
  const bladeMaterial = new THREE.MeshStandardMaterial({ color: 0x333333 });
  const blade1 = new THREE.Mesh(new THREE.BoxGeometry(0.02, 0.34, 0.01), bladeMaterial);
  const blade2 = blade1.clone();
  blade2.rotation.z = Math.PI / 2;
  propGroup.add(blade1, blade2);
  planeGroup.add(propGroup);

  // Wings (split left/right so each can carry its own aileron hinge).
  const wingSpan = 0.55;
  const wingDepth = 0.02;
  const wingChord = 0.2;
  const wingGeometry = new THREE.BoxGeometry(wingSpan, wingDepth, wingChord);

  const leftWing = new THREE.Mesh(wingGeometry, bodyMaterial);
  leftWing.position.set(-wingSpan / 2 - 0.06, 0, -0.05);
  planeGroup.add(leftWing);

  const rightWing = new THREE.Mesh(wingGeometry, bodyMaterial);
  rightWing.position.set(wingSpan / 2 + 0.06, 0, -0.05);
  planeGroup.add(rightWing);

  // Ailerons: hinge at each wing's outboard trailing edge, anchored to the
  // wingtip so widening extends the flap inward, not past the tip.
  const aileronWidth = 0.28;
  const leftAileronHinge = makeHingedFlap(aileronWidth, wingDepth, FLAP_DEPTH, aileronWidth / 2);
  leftAileronHinge.position.set(-wingSpan - 0.06, 0, -0.05 - wingChord / 2);
  planeGroup.add(leftAileronHinge);

  const rightAileronHinge = makeHingedFlap(aileronWidth, wingDepth, FLAP_DEPTH, -aileronWidth / 2);
  rightAileronHinge.position.set(wingSpan + 0.06, 0, -0.05 - wingChord / 2);
  planeGroup.add(rightAileronHinge);

  // Horizontal stabilizer + elevator, at the tail.
  const stabSpan = 0.32;
  const stabChord = 0.12;
  const hStab = new THREE.Mesh(
    new THREE.BoxGeometry(stabSpan, wingDepth, stabChord),
    bodyMaterial
  );
  hStab.position.set(0, 0, -0.46);
  planeGroup.add(hStab);

  const elevatorHinge = makeHingedFlap(stabSpan, wingDepth, FLAP_DEPTH, 0);
  elevatorHinge.position.set(0, 0, -0.46 - stabChord / 2);
  planeGroup.add(elevatorHinge);

  // Vertical fin + rudder, at the tail.
  const finHeight = 0.22;
  const finChord = 0.14;
  const fin = new THREE.Mesh(new THREE.BoxGeometry(0.02, finHeight, finChord), accentMaterial);
  fin.position.set(0, finHeight / 2, -0.46);
  planeGroup.add(fin);

  const rudderHinge = makeHingedFlap(0.02, finHeight, FLAP_DEPTH, 0);
  rudderHinge.position.set(0, finHeight / 2, -0.46 - finChord / 2);
  // rudder rotates about the vertical (Y) axis, unlike the other flaps.
  planeGroup.add(rudderHinge);

  // --- Animation state ------------------------------------------------------
  let currentValues = { motor: 0, roll: 0, yaw: 0, pitch: 0 };

  function update(values) {
    currentValues = values;
  }

  function resize() {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height, false);
  }

  let lastTime = performance.now();
  function animate() {
    requestAnimationFrame(animate);
    const now = performance.now();
    const dt = Math.min(0.1, (now - lastTime) / 1000);
    lastTime = now;

    const { motor, roll, yaw, pitch } = currentValues;

    // Propeller spin.
    propGroup.rotation.z += motor * LIMITS.propMaxSpeed * dt;

    // Control surface deflections. The plane body itself stays put — only
    // the propeller and flaps move.
    leftAileronHinge.rotation.x = deg2rad(roll * LIMITS.aileronDeg);
    rightAileronHinge.rotation.x = deg2rad(-roll * LIMITS.aileronDeg);
    elevatorHinge.rotation.x = deg2rad(pitch * LIMITS.elevatorDeg);
    rudderHinge.rotation.y = deg2rad(yaw * LIMITS.rudderDeg);

    controls.update();
    renderer.render(scene, camera);
  }

  new ResizeObserver(resize).observe(canvas);
  animate();

  return { update, LIMITS };
}
