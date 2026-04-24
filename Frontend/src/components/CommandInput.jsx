import { useRef } from 'react'
import '../styles/CommandInput.css'

function CommandInput({ commands, onCommandsChange, onExecute, onFileUpload, isLoading }) {
  const fileInputRef = useRef(null)

  const handleFileButtonClick = () => {
    fileInputRef.current?.click()
  }

  return (
    <div className="command-input">
      <div className="input-header">
        <h2>Área de Entrada de Comandos</h2>
      </div>

      <textarea
        className="command-textarea"
        value={commands}
        onChange={(e) => onCommandsChange(e.target.value)}
        placeholder="Ingresa los comandos aquí, uno por línea. Ejemplo:&#10;mkdisk -size=1000 -path=disk.mia&#10;mount -path=disk.mia -name=MIA&#10;mkdir -path=/home -name=carlos"
        disabled={isLoading}
      />

      <div className="button-group">
        <button
          className="btn btn-file"
          onClick={handleFileButtonClick}
          disabled={isLoading}
          title="Cargar archivo de script"
        >
          📁 Cargar Archivo
        </button>
        <input
          ref={fileInputRef}
          type="file"
          accept=".txt,.script"
          onChange={onFileUpload}
          style={{ display: 'none' }}
        />

        <button
          className="btn btn-execute"
          onClick={onExecute}
          disabled={isLoading || !commands.trim()}
          title="Ejecutar comandos"
        >
          {isLoading ? '⏳ Ejecutando...' : '▶ Ejecutar'}
        </button>
      </div>
    </div>
  )
}

export default CommandInput
