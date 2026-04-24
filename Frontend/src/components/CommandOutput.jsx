import { useEffect, useRef } from 'react'
import '../styles/CommandOutput.css'

function CommandOutput({ output }) {
  const outputRef = useRef(null)

  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight
    }
  }, [output])

  return (
    <div className="command-output">
      <div className="output-header">
        <h2>Área de Salida de Comandos</h2>
      </div>

      <div
        ref={outputRef}
        className="output-content"
      >
        {output ? (
          <pre>{output}</pre>
        ) : (
          <p className="output-placeholder">La salida de los comandos aparecerá aquí</p>
        )}
      </div>
    </div>
  )
}

export default CommandOutput
