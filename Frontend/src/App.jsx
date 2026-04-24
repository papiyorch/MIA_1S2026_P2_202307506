import { useState } from 'react'
import CommandInput from './components/CommandInput'
import CommandOutput from './components/CommandOutput'
import Login from './components/Login'
import Visualizer from './components/Visualizer'
import './App.css'

function App() {
  const [commands, setCommands] = useState('')
  const [output, setOutput] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  
  // Estado de sesión y vista
  const [isLoggedIn, setIsLoggedIn] = useState(false)
  const [currentView, setCurrentView] = useState('console') // 'console', 'login', 'visualizer'

  const handleExecute = async () => {
    if (!commands.trim()) {
      setOutput('Error: No hay comandos para ejecutar')
      return
    }

    setIsLoading(true)
    setOutput('Ejecutando comandos...')

    try {
      const response = await fetch('http://localhost:8000/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ commands: commands })
      })

      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`)

      const data = await response.json()
      setOutput(data.output || data.message || 'Comando ejecutado')
    } catch (error) {
      setOutput(`Error: ${error.message}`)
    } finally {
      setIsLoading(false)
    }
  }

  const handleFileUpload = (event) => {
    const file = event.target.files[0]
    if (file) {
      const reader = new FileReader()
      reader.onload = (e) => setCommands(e.target.result)
      reader.readAsText(file)
    }
  }

  const handleLogout = async () => {
    try {
      await fetch('http://localhost:8000/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ commands: 'logout' })
      })
    } catch (e) { console.error(e) }
    setIsLoggedIn(false)
    setCurrentView('console')
  }

  return (
    <div className="app-container">
      {/* Header principal */}
      <header className="app-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 20px' }}>
        <div>
          <h1>EXT2 Filesystem Simulator</h1>
          <p>Gestor de Comandos para Simulador de Sistema de Archivos</p>
        </div>
        
        {/* Botones por estado */}
        <div style={{ display: 'flex', gap: '10px' }}>
          {!isLoggedIn ? (
            <button 
              onClick={() => setCurrentView('login')}
              style={{ backgroundColor: '#28a745', color: 'white', padding: '10px 20px', border: 'none', borderRadius: '4px', cursor: 'pointer' }}
            >
              Iniciar Sesión
            </button>
          ) : (
            <>
              <button 
                onClick={() => setCurrentView(currentView === 'console' ? 'visualizer' : 'console')}
                style={{ backgroundColor: '#17a2b8', color: 'white', padding: '10px 20px', border: 'none', borderRadius: '4px', cursor: 'pointer' }}
              >
                {currentView === 'console' ? 'Ir al Visualizador' : 'Volver a Consola'}
              </button>
              <button 
                onClick={handleLogout}
                style={{ backgroundColor: '#dc3545', color: 'white', padding: '10px 20px', border: 'none', borderRadius: '4px', cursor: 'pointer' }}
              >
                Cerrar Sesión
              </button>
            </>
          )}
        </div>
      </header>

      <main className="app-main">
        {/* Vistas */}
        
        {currentView === 'login' && (
          <Login 
            onLoginSuccess={() => { setIsLoggedIn(true); setCurrentView('console'); }} 
            onCancel={() => setCurrentView('console')}
          />
        )}

        {currentView === 'console' && (
          <div className="panels">
            <div className="panel input-panel">
              <CommandInput
                commands={commands}
                onCommandsChange={setCommands}
                onExecute={handleExecute}
                onFileUpload={handleFileUpload}
                isLoading={isLoading}
              />
            </div>
            <div className="panel output-panel">
              <CommandOutput output={output} />
            </div>
          </div>
        )}

        {currentView === 'visualizer' && (
          <div style={{ padding: '20px', maxWidth: '1000px', margin: '0 auto' }}>
            <Visualizer />
          </div>
        )}

      </main>

      <footer className="app-footer">
        <p>© 2026 - EXT2 Filesystem Simulator | Frontend React + Vite</p>
      </footer>
    </div>
  )
}

export default App