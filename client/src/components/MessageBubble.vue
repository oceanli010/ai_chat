<template>
  <div :class="['message-bubble', message.role === 'user' ? 'bubble-user' : 'bubble-assistant']">
    <div class="bubble-avatar">
      {{ message.role === 'user' ? '👤' : '🤖' }}
    </div>
    <div class="bubble-body">
      <div class="bubble-header">
        <span class="bubble-role">{{ message.role === 'user' ? '你' : 'AI' }}</span>
        <span class="bubble-time">{{ formatTime(message.timestamp) }}</span>
      </div>
      <div class="bubble-content">{{ message.content }}</div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  message: {
    type: Object,
    required: true,
    validator(value) {
      return ['user', 'assistant'].includes(value.role) && typeof value.content === 'string'
    }
  }
})

function formatTime(timestamp) {
  const date = new Date(timestamp)
  const now = new Date()
  const isToday = date.toDateString() === now.toDateString()
  const hours = String(date.getHours()).padStart(2, '0')
  const minutes = String(date.getMinutes()).padStart(2, '0')
  if (isToday) {
    return `${hours}:${minutes}`
  }
  const month = String(date.getMonth() + 1).padStart(2, '0')
  const day = String(date.getDate()).padStart(2, '0')
  return `${month}-${day} ${hours}:${minutes}`
}
</script>

<style scoped>
.message-bubble {
  display: flex;
  gap: 12px;
  max-width: 80%;
}

.bubble-user {
  align-self: flex-end;
  flex-direction: row-reverse;
}

.bubble-assistant {
  align-self: flex-start;
}

.bubble-avatar {
  width: 36px;
  height: 36px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
  flex-shrink: 0;
  background: #f3f4f6;
}

.bubble-body {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.bubble-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 4px;
}

.bubble-role {
  font-size: 12px;
  font-weight: 600;
  color: #888;
}

.bubble-user .bubble-role {
  text-align: right;
}

.bubble-time {
  font-size: 11px;
  color: #bbb;
}

.bubble-content {
  padding: 10px 14px;
  border-radius: 12px;
  font-size: 14px;
  line-height: 1.6;
  word-break: break-word;
}

.bubble-user .bubble-content {
  background: #4f46e5;
  color: #fff;
  border-bottom-right-radius: 4px;
}

.bubble-assistant .bubble-content {
  background: #fff;
  color: #333;
  border-bottom-left-radius: 4px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.08);
}
</style>