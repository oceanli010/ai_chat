<template>
  <div class="admin-page">
    <div class="admin-sidebar">
      <h3>管理面板</h3>
      <div class="stats" v-if="stats">
        <div class="stat-item">
          <span class="stat-value">{{ stats.total_users }}</span>
          <span class="stat-label">总用户</span>
        </div>
        <div class="stat-item">
          <span class="stat-value">{{ stats.online_users }}</span>
          <span class="stat-label">在线</span>
        </div>
        <div class="stat-item">
          <span class="stat-value">{{ stats.banned_users }}</span>
          <span class="stat-label">已封禁</span>
        </div>
      </div>
    </div>

    <div class="admin-main">
      <div class="admin-toolbar">
        <input
          v-model="searchQuery"
          type="text"
          placeholder="搜索用户邮箱或昵称..."
          class="search-input"
          @input="debounceSearch"
        />
        <button class="btn btn-secondary" @click="refreshList">刷新</button>
      </div>

      <UserList
        :users="users"
        :loading="loading"
        @ban="handleBan"
      />

      <div class="pagination" v-if="totalPages > 1">
        <button
          class="btn-page"
          :disabled="currentPage <= 1"
          @click="changePage(currentPage - 1)"
        >上一页</button>
        <span class="page-info">{{ currentPage }} / {{ totalPages }}</span>
        <button
          class="btn-page"
          :disabled="currentPage >= totalPages"
          @click="changePage(currentPage + 1)"
        >下一页</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'
import api from '../services/api'
import UserList from '../components/UserList.vue'

const router = useRouter()
const userStore = useUserStore()

const users = ref([])
const loading = ref(false)
const searchQuery = ref('')
const currentPage = ref(1)
const totalPages = ref(1)
const stats = ref(null)

let searchTimer = null

async function fetchUsers(page = 1) {
  loading.value = true
  try {
    const result = await api.admin.getUsers(page, 20, searchQuery.value)
    users.value = result.users || []
    currentPage.value = page
    totalPages.value = result.total_pages || 1
  } catch (e) {
    console.error('Failed to fetch users:', e)
  } finally {
    loading.value = false
  }
}

async function fetchStats() {
  try {
    const result = await api.admin.getStats()
    stats.value = result
  } catch (e) {
    console.error('Failed to fetch stats:', e)
  }
}

async function handleBan(userId, banned) {
  try {
    await api.admin.banUser(userId, banned)
    await fetchUsers(currentPage.value)
    await fetchStats()
  } catch (e) {
    console.error('Failed to ban/unban user:', e)
  }
}

function debounceSearch() {
  if (searchTimer) clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    currentPage.value = 1
    fetchUsers(1)
  }, 300)
}

function changePage(page) {
  fetchUsers(page)
}

function refreshList() {
  fetchUsers(currentPage.value)
  fetchStats()
}

onMounted(() => {
  if (!userStore.token || !userStore.isAdmin) {
    router.push('/login')
    return
  }
  fetchUsers()
  fetchStats()
})

onUnmounted(() => {
  if (searchTimer) clearTimeout(searchTimer)
})
</script>

<style scoped>
.admin-page {
  flex: 1;
  display: flex;
  min-height: 0;
}

.admin-sidebar {
  width: 220px;
  background: #1e293b;
  color: #fff;
  padding: 20px;
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
}

.admin-sidebar h3 {
  font-size: 16px;
  font-weight: 600;
  margin-bottom: 20px;
  padding-bottom: 12px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.stats {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.stat-item {
  display: flex;
  flex-direction: column;
  padding: 12px;
  background: rgba(255, 255, 255, 0.08);
  border-radius: 8px;
}

.stat-value {
  font-size: 24px;
  font-weight: 700;
}

.stat-label {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
  margin-top: 4px;
}

.admin-main {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.admin-toolbar {
  display: flex;
  gap: 12px;
  padding: 16px 20px;
  background: #fff;
  border-bottom: 1px solid #e5e7eb;
  align-items: center;
}

.search-input {
  flex: 1;
  height: 38px;
  padding: 0 12px;
  border: 1px solid #d9d9d9;
  border-radius: 8px;
  font-size: 14px;
  outline: none;
  transition: border-color 0.2s;
}

.search-input:focus {
  border-color: #4f46e5;
}

.btn {
  height: 38px;
  padding: 0 16px;
  border: none;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
  white-space: nowrap;
}

.btn-secondary {
  background: #f3f4f6;
  color: #555;
}

.btn-secondary:hover {
  background: #e5e7eb;
}

.pagination {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 16px;
  padding: 16px;
  background: #fff;
  border-top: 1px solid #e5e7eb;
}

.btn-page {
  height: 34px;
  padding: 0 14px;
  border: 1px solid #d9d9d9;
  border-radius: 6px;
  background: #fff;
  font-size: 13px;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-page:hover:not(:disabled) {
  border-color: #4f46e5;
  color: #4f46e5;
}

.btn-page:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.page-info {
  font-size: 13px;
  color: #888;
}
</style>