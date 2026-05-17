class WebSocketClient {
  constructor() {
    this._ws = null
    this._url = ''
    this._handlers = new Map()
    this._reconnectTimer = null
    this._reconnectAttempts = 0
    this._maxReconnectAttempts = 10
    this._reconnectDelay = 1000
    this._intentionalClose = false
    this._connected = false
  }

  get connected() {
    return this._connected
  }

  connect(token) {
    const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws'
    const host = window.location.hostname
    this._url = `${protocol}://${host}:8081/ws?token=${encodeURIComponent(token)}`
    this._intentionalClose = false
    this._connect()
  }

  _connect() {
    if (this._ws && (this._ws.readyState === WebSocket.OPEN || this._ws.readyState === WebSocket.CONNECTING)) {
      return
    }

    try {
      this._ws = new WebSocket(this._url)
    } catch (e) {
      this._scheduleReconnect()
      return
    }

    this._ws.onopen = () => {
      this._connected = true
      this._reconnectAttempts = 0
      this._reconnectDelay = 1000
      this._emit('open')
    }

    this._ws.onmessage = (event) => {
      try {
        const message = JSON.parse(event.data)
        const type = message.type
        if (type && this._handlers.has(type)) {
          this._handlers.get(type)(message.data)
        }
        this._emit('message', message)
      } catch (e) {
        this._emit('error', { message: 'Invalid message format' })
      }
    }

    this._ws.onclose = () => {
      this._connected = false
      this._emit('close')
      if (!this._intentionalClose) {
        this._scheduleReconnect()
      }
    }

    this._ws.onerror = () => {
      this._emit('error', { message: 'WebSocket connection error' })
    }
  }

  disconnect() {
    this._intentionalClose = true
    if (this._reconnectTimer) {
      clearTimeout(this._reconnectTimer)
      this._reconnectTimer = null
    }
    if (this._ws) {
      this._ws.close()
      this._ws = null
    }
    this._connected = false
  }

  send(type, data = {}) {
    if (!this._ws || this._ws.readyState !== WebSocket.OPEN) {
      return false
    }
    const message = JSON.stringify({ type, data })
    this._ws.send(message)
    return true
  }

  on(type, handler) {
    this._handlers.set(type, handler)
  }

  off(type) {
    this._handlers.delete(type)
  }

  _on(eventType, handler) {
    if (!this._eventHandlers) {
      this._eventHandlers = new Map()
    }
    if (!this._eventHandlers.has(eventType)) {
      this._eventHandlers.set(eventType, [])
    }
    this._eventHandlers.get(eventType).push(handler)
  }

  _emit(eventType, data) {
    if (this._eventHandlers && this._eventHandlers.has(eventType)) {
      for (const handler of this._eventHandlers.get(eventType)) {
        handler(data)
      }
    }
  }

  _scheduleReconnect() {
    if (this._reconnectAttempts >= this._maxReconnectAttempts) {
      this._emit('max_reconnect')
      return
    }
    this._reconnectAttempts++
    this._reconnectTimer = setTimeout(() => {
      this._connect()
    }, this._reconnectDelay)
    this._reconnectDelay = Math.min(this._reconnectDelay * 1.5, 30000)
  }
}

const wsClient = new WebSocketClient()
export default wsClient