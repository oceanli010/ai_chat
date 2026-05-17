<template>
  <div class="user-list">
    <div v-if="loading" class="loading">加载中...</div>
    <div v-else-if="users.length === 0" class="empty">暂无用户数据</div>
    <div v-else class="list">
      <div v-for="user in users" :key="user.id" class="user-row">
        <div class="user-info">
          <span class="user-status" :class="user.status"></span>
          <div class="user-detail">
            <span class="user-nickname">{{ user.nickname }}</span>
            <span class="user-email">{{ user.email }}</span>
          </div>
          <span class="user-role">{{ user.role === 'admin' ? '管理员' : '用户' }}</span>
        </div>
        <div class="user-actions">
          <button
            v-if="user.role !== 'admin'"
            :class="['btn-action', user.status === 'banned' ? 'btn-unban' : 'btn-ban']"
            @click="$emit('ban', user.id, user.status !== 'banned')"
          >
            {{ user.status === 'banned' ? '解封' : '封禁' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  users: {
    type: Array,
    default: () => []
  },
  loading: {
    type: Boolean,
    default: false
  }
})

defineEmits(['ban'])
</script>

<style scoped>
.user-list {
  flex: 1;
  overflow-y: auto;
}

.loading,
.empty {
  padding: 40px 20px;
  text-align: center;
  color: #aaa;
  font-size: 14px;
}

.list {
  display: flex;
  flex-direction: column;
}

.user-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 20px;
  border-bottom: 1px solid #f0f0f0;
  transition: background 0.15s;
}

.user-row:hover {
  background: #fafbfc;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 12px;
  flex: 1;
  min-width: 0;
}

.user-status {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}

.user-status.active {
  background: #22c55e;
}

.user-status.banned {
  background: #ef4444;
}

.user-detail {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.user-nickname {
  font-size: 14px;
  font-weight: 500;
  color: #333;
}

.user-email {
  font-size: 12px;
  color: #999;
}

.user-role {
  font-size: 12px;
  padding: 2px 8px;
  background: #f0f0f0;
  border-radius: 4px;
  color: #666;
  flex-shrink: 0;
}

.user-actions {
  display: flex;
  gap: 8px;
  margin-left: 16px;
  flex-shrink: 0;
}

.btn-action {
  height: 30px;
  padding: 0 12px;
  border: none;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-ban {
  background: #fef2f2;
  color: #dc2626;
}

.btn-ban:hover {
  background: #fee2e2;
}

.btn-unban {
  background: #f0fdf4;
  color: #16a34a;
}

.btn-unban:hover {
  background: #dcfce7;
}
</style>