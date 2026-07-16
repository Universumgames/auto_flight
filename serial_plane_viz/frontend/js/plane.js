import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
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
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.1;
  renderer.outputColorSpace = THREE.SRGBColorSpace;

  // Soft image-based environment lighting so standard materials pick up
  // gentle reflections/highlights instead of looking flat and matte.
  const pmremGenerator = new THREE.PMREMGenerator(renderer);
  scene.environment = pmremGenerator.fromScene(new RoomEnvironment(), 0.04).texture;

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.target.set(0, 0, 0);
  controls.enableDamping = true;

  // Dim ambient fill so shadows read clearly, plus a key "sun" light casting
  // shadows and a soft rim/fill light from the opposite side for depth.
  scene.add(new THREE.HemisphereLight(0xffffff, 0x33363f, 0.5));

  const sun = new THREE.DirectionalLight(0xffffff, 2.2);
  sun.position.set(2, 3, 1);
  sun.castShadow = true;
  sun.shadow.mapSize.set(1024, 1024);
  sun.shadow.camera.near = 0.1;
  sun.shadow.camera.far = 8;
  sun.shadow.camera.left = -1.2;
  sun.shadow.camera.right = 1.2;
  sun.shadow.camera.top = 1.2;
  sun.shadow.camera.bottom = -1.2;
  sun.shadow.bias = -0.0015;
  sun.shadow.radius = 2;
  scene.add(sun);

  const fill = new THREE.DirectionalLight(0xaeccff, 0.35);
  fill.position.set(-2, 1, -1.5);
  scene.add(fill);

  const grid = new THREE.GridHelper(6, 24, 0x444a55, 0x2b2f38);
  scene.add(grid);

  const ground = new THREE.Mesh(
    new THREE.PlaneGeometry(6, 6),
    new THREE.ShadowMaterial({ opacity: 0.35 })
  );
  ground.rotation.x = -Math.PI / 2;
  ground.position.y = -0.001;
  ground.receiveShadow = true;
  scene.add(ground);

  // --- Plane construction -------------------------------------------------
  const bodyMaterial = new THREE.MeshStandardMaterial({ color: 0xe8503a, roughness: 0.45, metalness: 0.1 });
  const accentMaterial = new THREE.MeshStandardMaterial({ color: 0xf0f0f0, roughness: 0.35, metalness: 0.1 });
  const canopyMaterial = new THREE.MeshStandardMaterial({
    color: 0x8fd0ff,
    roughness: 0.15,
    metalness: 0.2,
    transparent: true,
    opacity: 0.55,
  });
  const gearMaterial = new THREE.MeshStandardMaterial({ color: 0x2a2a2a, roughness: 0.6, metalness: 0.4 });
  const wheelMaterial = new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.9 });

  const planeGroup = new THREE.Group();
  scene.add(planeGroup);

  // Fuselage: a short, rounded body of revolution (nose to tail) instead of
  // a plain box, for a plump, more aerodynamic silhouette. Nose points
  // toward +Z.
  const fuselageProfile = [
    new THREE.Vector2(0.0, -0.39),
    new THREE.Vector2(0.03, -0.34),
    new THREE.Vector2(0.06, -0.15),
    new THREE.Vector2(0.075, 0.04),
    new THREE.Vector2(0.065, 0.22),
    new THREE.Vector2(0.04, 0.37),
    new THREE.Vector2(0.0, 0.46),
  ];
  const fuselageGeometry = new THREE.LatheGeometry(fuselageProfile, 14);
  fuselageGeometry.rotateX(Math.PI / 2);
  const fuselage = new THREE.Mesh(fuselageGeometry, bodyMaterial);
  planeGroup.add(fuselage);

  // Canopy bubble over the cockpit.
  const canopy = new THREE.Mesh(
    new THREE.SphereGeometry(0.055, 16, 10, 0, Math.PI * 2, 0, Math.PI / 2),
    canopyMaterial
  );
  canopy.scale.set(0.85, 0.65, 1.5);
  canopy.position.set(0, 0.055, 0.08);
  planeGroup.add(canopy);

  // Propeller.
  const propGroup = new THREE.Group();
  propGroup.position.z = 0.49;
  const hub = new THREE.Mesh(
    new THREE.CylinderGeometry(0.015, 0.015, 0.05, 8),
    new THREE.MeshStandardMaterial({ color: 0x222222, roughness: 0.4, metalness: 0.7 })
  );
  hub.rotation.x = Math.PI / 2;
  propGroup.add(hub);
  const bladeMaterial = new THREE.MeshStandardMaterial({ color: 0x333333, roughness: 0.4, metalness: 0.6 });
  const blade1 = new THREE.Mesh(new THREE.BoxGeometry(0.02, 0.34, 0.01), bladeMaterial);
  const blade2 = blade1.clone();
  blade2.rotation.z = Math.PI / 2;
  propGroup.add(blade1, blade2);
  planeGroup.add(propGroup);

  // Wings: a full-chord root segment plus a tapered tip segment and a small
  // winglet, split left/right so each can carry its own aileron hinge.
  const wingSpan = 0.55; // root-to-tip span per side
  const wingDepth = 0.02;
  const wingChord = 0.2;
  const wingInnerX = 0.075; // gap from fuselage centerline to wing root
  const wingRootSpan = wingSpan * 0.62;
  const wingTipSpan = wingSpan - wingRootSpan;
  const wingTipChord = wingChord * 0.6;

  const wingRootGeometry = new THREE.BoxGeometry(wingRootSpan, wingDepth, wingChord);
  const wingTipGeometry = new THREE.BoxGeometry(wingTipSpan, wingDepth * 0.8, wingTipChord);
  const wingletGeometry = new THREE.BoxGeometry(0.006, 0.05, wingTipChord * 0.8);

  function buildWing(sign) {
    const root = new THREE.Mesh(wingRootGeometry, bodyMaterial);
    root.position.set(sign * (wingInnerX + wingRootSpan / 2), 0, -0.04);
    planeGroup.add(root);

    const tip = new THREE.Mesh(wingTipGeometry, bodyMaterial);
    tip.position.set(sign * (wingInnerX + wingRootSpan + wingTipSpan / 2), 0, -0.04);
    planeGroup.add(tip);

    const winglet = new THREE.Mesh(wingletGeometry, accentMaterial);
    winglet.position.set(sign * (wingInnerX + wingRootSpan + wingTipSpan), 0.025, -0.04);
    planeGroup.add(winglet);
  }
  buildWing(-1);
  buildWing(1);

  // Ailerons: hinge at each wing's outboard trailing edge, anchored to the
  // wingtip so widening extends the flap inward, not past the tip. Offset
  // from the tapered tip's (narrower) chord so the flap sits flush.
  const aileronWidth = 0.28;
  const leftAileronHinge = makeHingedFlap(aileronWidth, wingDepth, FLAP_DEPTH, aileronWidth / 2);
  leftAileronHinge.position.set(-wingSpan - wingInnerX, 0, -0.04 - wingTipChord / 2);
  planeGroup.add(leftAileronHinge);

  const rightAileronHinge = makeHingedFlap(aileronWidth, wingDepth, FLAP_DEPTH, -aileronWidth / 2);
  rightAileronHinge.position.set(wingSpan + wingInnerX, 0, -0.04 - wingTipChord / 2);
  planeGroup.add(rightAileronHinge);

  // Horizontal stabilizer + elevator, at the tail: a center root segment
  // plus tapered tip segments on each side.
  const stabSpan = 0.32;
  const stabChord = 0.12;
  const stabZ = -0.34;
  const stabRootSpan = stabSpan * 0.55;
  const stabTipSpan = (stabSpan - stabRootSpan) / 2;
  const stabTipChord = stabChord * 0.6;

  const hStabRoot = new THREE.Mesh(new THREE.BoxGeometry(stabRootSpan, wingDepth, stabChord), bodyMaterial);
  hStabRoot.position.set(0, 0, stabZ);
  planeGroup.add(hStabRoot);

  [-1, 1].forEach((sign) => {
    const stabTip = new THREE.Mesh(
      new THREE.BoxGeometry(stabTipSpan, wingDepth * 0.8, stabTipChord),
      bodyMaterial
    );
    stabTip.position.set(sign * (stabRootSpan / 2 + stabTipSpan / 2), 0, stabZ);
    planeGroup.add(stabTip);
  });

  const elevatorHinge = makeHingedFlap(stabSpan, wingDepth, FLAP_DEPTH, 0);
  elevatorHinge.position.set(0, 0, stabZ - stabTipChord / 2);
  planeGroup.add(elevatorHinge);

  // Vertical fin + rudder, at the tail: a root segment plus a swept, tapered
  // tip segment.
  const finHeight = 0.22;
  const finChord = 0.14;
  const finZ = -0.34;
  const finRootHeight = finHeight * 0.65;
  const finTipHeight = finHeight - finRootHeight;
  const finTipChord = finChord * 0.55;

  const finRoot = new THREE.Mesh(new THREE.BoxGeometry(0.02, finRootHeight, finChord), accentMaterial);
  finRoot.position.set(0, finRootHeight / 2, finZ);
  planeGroup.add(finRoot);

  const finTip = new THREE.Mesh(new THREE.BoxGeometry(0.02, finTipHeight, finTipChord), accentMaterial);
  finTip.position.set(0, finRootHeight + finTipHeight / 2, finZ);
  planeGroup.add(finTip);

  const rudderHinge = makeHingedFlap(0.02, finHeight, FLAP_DEPTH, 0);
  rudderHinge.position.set(0, finHeight / 2, finZ - finTipChord / 2);
  // rudder rotates about the vertical (Y) axis, unlike the other flaps.
  planeGroup.add(rudderHinge);

  // Fixed taildragger landing gear.
  const mainGearZ = -0.02;
  const mainGearX = 0.11;
  const strutLength = 0.09;
  const wheelRadius = 0.035;

  [-1, 1].forEach((sign) => {
    const strut = new THREE.Mesh(new THREE.CylinderGeometry(0.006, 0.006, strutLength, 6), gearMaterial);
    strut.position.set(sign * mainGearX, -strutLength / 2 - 0.02, mainGearZ);
    strut.rotation.z = sign * deg2rad(8);
    planeGroup.add(strut);

    const wheel = new THREE.Mesh(new THREE.CylinderGeometry(wheelRadius, wheelRadius, 0.02, 14), wheelMaterial);
    wheel.rotation.z = Math.PI / 2;
    wheel.position.set(sign * mainGearX, -strutLength - 0.02, mainGearZ);
    planeGroup.add(wheel);
  });

  const tailStrut = new THREE.Mesh(new THREE.CylinderGeometry(0.004, 0.004, 0.04, 6), gearMaterial);
  tailStrut.position.set(0, -0.02, -0.375);
  planeGroup.add(tailStrut);

  const tailWheel = new THREE.Mesh(new THREE.CylinderGeometry(0.015, 0.015, 0.012, 10), wheelMaterial);
  tailWheel.rotation.z = Math.PI / 2;
  tailWheel.position.set(0, -0.04, -0.375);
  planeGroup.add(tailWheel);

  planeGroup.traverse((obj) => {
    if (obj.isMesh) {
      obj.castShadow = true;
      obj.receiveShadow = true;
    }
  });
  canopy.castShadow = false;

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
