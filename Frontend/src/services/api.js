import { API_CONFIG, API_ENDPOINTS } from '../config/api'

export const apiService = {
  async executeCommands(commands) {
    try {
      const response = await fetch(
        `${API_CONFIG.BASE_URL}${API_ENDPOINTS.EXECUTE}`,
        {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ commands }),
        }
      )

      if (!response.ok) {
        throw new Error(`API Error: ${response.status} ${response.statusText}`)
      }

      const data = await response.json()
      return data
    } catch (error) {
      console.error('API Error:', error)
      throw error
    }
  },

  async checkHealth() {
    try {
      const response = await fetch(
        `${API_CONFIG.BASE_URL}${API_ENDPOINTS.HEALTH}`
      )
      return response.ok
    } catch {
      return false
    }
  },
}
