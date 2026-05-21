<template>
  <nav class="navbar">
    <div class="navbar-brand">
      <router-link to="/chat" class="brand-link">AI 聊天室</router-link>
    </div>
    <div class="navbar-links">
      <router-link to="/chat" class="nav-link">聊天</router-link>
      <router-link to="/settings" class="nav-link">设置</router-link>
      <router-link v-if="userStore.isAdmin" to="/admin" class="nav-link">管理</router-link>
    </div>
    <div class="navbar-user">
      <span v-if="userStore.user" class="user-name">{{ userStore.user.nickname || userStore.user.email }}</span>
      <button v-if="userStore.isLoggedIn" class="btn-logout" @click="handleLogout">退出</button>
    </div>
  </nav>
</template>

<script setup>
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'

const router = useRouter()
const userStore = useUserStore()

async function handleLogout() {
  await userStore.logout()
  router.push('/login')
}
</script>

<style scoped>
.navbar {
  display: flex;
  align-items: center;
  padding: 0 20px;
  height: 52px;
  background: #fff;
  border-bottom: 1px solid #e5e7eb;
  flex-shrink: 0;
}

.navbar-brand {
  margin-right: 32px;
}

.brand-link {
  font-size: 16px;
  font-weight: 700;
  color: #4f46e5;
  text-decoration: none;
}

.navbar-links {
  display: flex;
  gap: 8px;
  flex: 1;
}

.nav-link {
  padding: 6px 14px;
  border-radius: 6px;
  font-size: 14px;
  color: #555;
  text-decoration: none;
  transition: all 0.2s;
}

.nav-link:hover {
  background: #f3f4f6;
  color: #333;
}

.nav-link.router-link-active {
  background: #eef2ff;
  color: #4f46e5;
}

.navbar-user {
  display: flex;
  align-items: center;
  gap: 12px;
}

.user-name {
  font-size: 13px;
  color: #666;
  max-width: 150px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.btn-logout {
  height: 30px;
  padding: 0 14px;
  border: 1px solid #e5e7eb;
  border-radius: 6px;
  background: #fff;
  font-size: 13px;
  color: #888;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-logout:hover {
  border-color: #dc2626;
  color: #dc2626;
}
</style>