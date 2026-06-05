import { createRouter, createWebHistory } from 'vue-router'
import HomeView from '../views/HomeView.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      name: 'home',
      component: HomeView,
    },
    {
      path: '/about',
      name: 'about',
      // route level code-splitting
      // this generates a separate chunk (About.[hash].js) for this route
      // which is lazy-loaded when the route is visited.
      component: () => import('../views/AboutView.vue'),
    },
    {
      path:"/connection",
      name: "Connection",
      component: () => import('../views/ConnectionOverview.vue')
    },
    {
      path: '/map',
      name: "Map",
      component: () => import('../views/MapView.vue'),
    },
    {
      path: '/area',
      name: 'AreaPlanner',
      component: () => import('../views/AreaPlanner.vue'),
    },
    {
      path:"/route",
      name: "RoutePreview",
      component: () => import("../views/RoutePreviewView.vue")
    }
  ],
})

export default router
