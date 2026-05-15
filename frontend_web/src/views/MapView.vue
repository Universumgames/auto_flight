<script setup lang="ts">
import 'leaflet/dist/leaflet.css'
import { LMap, LTileLayer, LMarker, LPolyline } from '@vue-leaflet/vue-leaflet'
import { onBeforeMount, ref } from 'vue'
import { store } from '@/stores/store.ts'
import type { Coordinate } from '@/types/coordinates.ts'

const zoom = ref(15)

const coordinates = ref<Coordinate>({
  latitude: 51.316310347903176,
  longitude: 6.569530261539499,
})

</script>

<template>
  <div style="height: 600px; width: 800px">
    <l-map
      ref="map"
      v-model:zoom="zoom"
      :center="[store.basePosition?.latitude ?? 0, store.basePosition?.longitude ?? 0]"
    >
      <l-tile-layer
        url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        layer-type="base"
        name="OpenStreetMap"
      ></l-tile-layer>
      <l-marker :lat-lng="[coordinates.latitude, coordinates.longitude]"></l-marker>
      <l-marker
        v-if="store.basePosition != null"
        :lat-lng="[store.basePosition.latitude, store.basePosition.longitude]"
      ></l-marker
      >
      <l-polyline
        :lat-lngs="[
          [47.334852, -1.509485],
          [47.342596, -1.328731],
          [47.241487, -1.190568],
          [47.234787, -1.358337],
        ]"
        color="green"
      />
    </l-map>
  </div>
</template>

<style scoped></style>
