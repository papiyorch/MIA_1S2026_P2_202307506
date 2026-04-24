import { useState } from 'react';

export default function Login({ onLoginSuccess, onCancel }) {
  const [partId, setPartId] = useState('');
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState('');

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!partId || !username || !password) {
      setError('Todos los campos son obligatorios');
      return;
    }

    setIsLoading(true);
    setError('');

    try {
      // Login al endpoint
      const response = await fetch('http://localhost:8000/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ commands: `login -user=${username} -pass=${password} -id=${partId}` })
      });

      const data = await response.json();
      const salida = data.output || data.message || '';

      if (salida.toLowerCase().includes('exitoso') || salida.toLowerCase().includes('bienvenido')) {
        onLoginSuccess();
      } else {
        setError(salida);
      }
    } catch (err) {
      setError('Error de conexión con el backend.');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '60vh' }}>
      <div style={{ backgroundColor: 'white', padding: '40px', borderRadius: '4px', border: '1px solid #ddd', width: '550px', boxShadow: '0 2px 4px rgba(0,0,0,0.1)' }}>
        
        {/* Regresar */}
        <button onClick={onCancel} style={{ border: 'none', background: 'none', color: '#007bff', cursor: 'pointer', marginBottom: '10px' }}>
          ← Volver a Consola
        </button>

        <h2 style={{ textAlign: 'center', color: '#6c757d', marginBottom: '20px' }}>Login</h2>
        
        {error && <div style={{ color: '#721c24', backgroundColor: '#f8d7da', padding: '10px', borderRadius: '4px', marginBottom: '15px', fontSize: '14px' }}>{error}</div>}

        <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '15px' }}>
          <div>
            <label style={{ fontSize: '12px', color: '#333' }}>ID Particion</label>
            <input type="text" value={partId} onChange={(e) => setPartId(e.target.value)} style={{ width: '100%', padding: '8px', backgroundColor: '#e9ecef', border: '1px solid #ced4da', borderRadius: '4px', boxSizing: 'border-box' }} />
          </div>

          <div>
            <label style={{ fontSize: '12px', color: '#333' }}>Usuario:</label>
            <input type="text" value={username} onChange={(e) => setUsername(e.target.value)} style={{ width: '100%', padding: '8px', backgroundColor: '#e9ecef', border: '1px solid #ced4da', borderRadius: '4px', boxSizing: 'border-box' }} />
          </div>

          <div>
            <label style={{ fontSize: '12px', color: '#333' }}>Contraseña:</label>
            <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} style={{ width: '100%', padding: '8px', backgroundColor: '#e9ecef', border: '1px solid #ced4da', borderRadius: '4px', boxSizing: 'border-box' }} />
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}>
            <input type="checkbox" id="recordar" />
            <label htmlFor="recordar" style={{ fontSize: '12px', color: '#6c757d' }}>Recordar usuario</label>
          </div>

          <button type="submit" disabled={isLoading} style={{ backgroundColor: '#007bff', color: 'white', padding: '10px', border: 'none', borderRadius: '4px', cursor: 'pointer', fontWeight: 'bold' }}>
            {isLoading ? 'Cargando...' : 'Submit'}
          </button>
        </form>
      </div>
    </div>
  );
}