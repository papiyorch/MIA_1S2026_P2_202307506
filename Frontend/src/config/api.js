// Config API
export const API_CONFIG = {
  BASE_URL: process.env.REACT_APP_API_URL || 'http://localhost:8000',
  TIMEOUT: 30000, // timeout ms
};

export const API_ENDPOINTS = {
  EXECUTE: '/execute',
  STATUS: '/status',
  HEALTH: '/health',
};
