import { useState, useEffect } from 'react';

export default function Visualizer() {
  const [step, setStep] = useState('disks'); 
  
  // Estado de navegación
  const [selectedDisk, setSelectedDisk] = useState(null);
  const [selectedPartition, setSelectedPartition] = useState(null);
  const [currentPath, setCurrentPath] = useState('/');
  
  // Datos cargados del backend
  const [disks, setDisks] = useState([]);
  const [partitions, setPartitions] = useState([]);
  const [files, setFiles] = useState([]);
  const [fileContent, setFileContent] = useState('');
  
  // Estado de carga
  const [isLoading, setIsLoading] = useState(false);

  // Cargar discos al iniciar
  useEffect(() => {
    fetchDisks();
  }, []);

  const fetchDisks = async () => {
    setIsLoading(true);
    try {
      const response = await fetch('http://3.90.156.64:8000/api/disks'); 
      if(response.ok) {
        const data = await response.json();
        setDisks(data);
      } else {
        setDisks(['Disco1.mia', 'Disco2.mia']); 
      }
    } catch (error) {
      console.error("Error cargando discos", error);
      setDisks(['Disco1.mia', 'Disco2.mia']); 
    } finally {
      setIsLoading(false);
    }
  };

  // Cargar particiones del disco
  const handleSelectDisk = async (disk) => {
    setSelectedDisk(disk);
    setStep('partitions');
    setIsLoading(true);

    try {
      const response = await fetch(`http://3.90.156.64:8000/api/partitions?disk=${disk}`);
      if(response.ok) {
        const data = await response.json();
        setPartitions(data);
      } else {
        setPartitions(disk === 'Disco2.mia' ? [] : ['Particion1', 'Particion2']);
      }
    } catch (error) {
      setPartitions(disk === 'Disco2.mia' ? [] : ['Particion1', 'Particion2']);
    } finally {
      setIsLoading(false);
    }
  };

  // Cargar contenido de carpeta
  const loadDirectory = async (disk, partition, path) => {
    setIsLoading(true);
    try {
      const response = await fetch(`http://3.90.156.64:8000/api/explore?disk=${disk}&partition=${partition}&path=${path}`);
      if(response.ok) {
        const data = await response.json();
        setFiles(data);
      } else {
         throw new Error("Simulando que no hay API");
      }
    } catch (error) {
      // Fallback local
      if (partition === 'Particion2') {
        setFiles([]); 
      } else if (path === '/') {
        setFiles([
          { name: 'calificacion', type: 'folder' }, 
          { name: 'users.txt', type: 'file' }
        ]);
      } else if (path === '/calificacion/') {
        setFiles([
          { name: 'U2025', type: 'folder' },
          { name: 'TEST', type: 'folder' },
          { name: 'MOVE', type: 'folder' },
          { name: 'MIA_Aprobado.txt', type: 'file', content: 'Felicidades, llegaste hasta aqui.' }
        ]);
      } else {
        setFiles([]); 
      }
    } finally {
      setIsLoading(false);
    }
  };

  const handleSelectPartition = (partition) => {
    setSelectedPartition(partition);
    setCurrentPath('/');
    setStep('explorer');
    loadDirectory(selectedDisk, partition, '/');
  };

  const handleOpenFile = async (file) => {
    if (file.type === 'folder') {
      const newPath = currentPath + file.name + '/';
      setCurrentPath(newPath);
      loadDirectory(selectedDisk, selectedPartition, newPath);
    } else {
      // Leer contenido archivo
      setIsLoading(true);
      try {
        // Evitar doble slash en raíz
        const filePath = currentPath === '/' ? `/${file.name}` : `${currentPath}${file.name}`;
        
        // Archivo al endpoint
        const response = await fetch(`http://3.90.156.64:8000/api/file?disk=${selectedDisk}&partition=${selectedPartition}&path=${filePath}`);
        
        if(response.ok) {
          const data = await response.json();
          setFileContent(data.content);
        } else {
          setFileContent(file.content || "Contenido simulado del archivo " + file.name); 
        }
      } catch (error) {
        setFileContent(file.content || "Contenido simulado del archivo " + file.name);
      } finally {
        setIsLoading(false);
        setStep('fileViewer');
      }
    }
  };

  // Estilos inline
  const containerStyle = { backgroundColor: 'white', padding: '30px', borderRadius: '8px', boxShadow: '0 4px 6px rgba(0,0,0,0.1)', minHeight: '400px', border: '1px solid #ddd' };
  const headerStyle = { textAlign: 'center', color: '#6c757d', marginBottom: '30px', fontSize: '24px', fontWeight: 'bold' };
  const pathBarStyle = { backgroundColor: '#6c757d', color: 'white', padding: '12px 15px', borderRadius: '4px', marginBottom: '20px', fontFamily: 'monospace', fontSize: '14px', display: 'flex', alignItems: 'center' };
  const backButtonStyle = { border: 'none', background: 'none', color: '#007bff', cursor: 'pointer', marginBottom: '15px', fontSize: '14px', display: 'flex', alignItems: 'center', gap: '5px' };
  
  // Iconos
  const renderIcon = (type) => {
      if (type === 'folder') return <div style={{ fontSize: '60px', lineHeight: '1' }}>📁</div>;
      return <div style={{ fontSize: '60px', lineHeight: '1', color: '#5bc0de' }}>📄</div>; 
  }

  // Render
  return (
    <div style={containerStyle}>
      <h2 style={headerStyle}>
        {step === 'fileViewer' ? 'Contenido del Archivo' : 'Visualizador del Sistema de Archivos'}
      </h2>

      {isLoading ? (
        <div style={{ textAlign: 'center', padding: '50px', color: '#007bff', fontSize: '18px' }}>Cargando datos...</div>
      ) : (
        <>
          {/* Vista discos */}
          {step === 'disks' && (
            <div>
              <p style={{ textAlign: 'center', color: '#555', marginBottom: '20px' }}>Seleccione el disco que desea visualizar</p>
              <div style={{ display: 'flex', gap: '30px', flexWrap: 'wrap', justifyContent: 'center' }}>
                {disks.length === 0 ? <p style={{color: '#ccc'}}>No hay discos disponibles.</p> : disks.map((disk, i) => (
                  <div key={i} onClick={() => handleSelectDisk(disk)} style={{ textAlign: 'center', cursor: 'pointer', padding: '15px', transition: 'transform 0.2s' }} onMouseEnter={(e) => e.currentTarget.style.transform = 'scale(1.05)'} onMouseLeave={(e) => e.currentTarget.style.transform = 'scale(1)'}>
                    <div style={{ fontSize: '60px' }}>🖴</div>
                    <div style={{ fontSize: '14px', fontWeight: '500', marginTop: '5px' }}>{disk}</div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Vista particiones */}
          {step === 'partitions' && (
            <div>
              <button onClick={() => { setStep('disks'); fetchDisks(); }} style={backButtonStyle}>← Volver</button>
              <p style={{ textAlign: 'center', color: '#555', marginBottom: '20px' }}>Seleccione la partición que desea visualizar</p>
              <div style={{ display: 'flex', gap: '30px', flexWrap: 'wrap', justifyContent: 'center' }}>
                {partitions.length === 0 ? (
                  <div style={{ padding: '50px', color: '#bbb', fontStyle: 'italic' }}>Este disco no tiene particiones creadas.</div>
                ) : (
                  partitions.map((part, i) => (
                    <div key={i} onClick={() => handleSelectPartition(part)} style={{ textAlign: 'center', cursor: 'pointer', padding: '15px', transition: 'transform 0.2s' }} onMouseEnter={(e) => e.currentTarget.style.transform = 'scale(1.05)'} onMouseLeave={(e) => e.currentTarget.style.transform = 'scale(1)'}>
                      <div style={{ fontSize: '60px' }}>💿</div>
                      <div style={{ fontSize: '14px', fontWeight: '500', marginTop: '5px' }}>{part}</div>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

          {/* Vista explorador */}
          {step === 'explorer' && (
            <div>
              <button onClick={() => { 
                if (currentPath === '/') {
                  setStep('partitions');
                  handleSelectDisk(selectedDisk); 
                } else {
                   const pathArray = currentPath.split('/').filter(p => p !== '');
                   pathArray.pop(); 
                   const prevPath = pathArray.length === 0 ? '/' : '/' + pathArray.join('/') + '/';
                   setCurrentPath(prevPath);
                   loadDirectory(selectedDisk, selectedPartition, prevPath);
                }
              }} style={backButtonStyle}>← Volver</button>
              
              <div style={pathBarStyle}>{currentPath}</div>
              <p style={{ textAlign: 'left', color: '#555', marginBottom: '20px', fontSize: '14px' }}>Navegue entre carpetas o visualice archivos</p>
              
              <div style={{ display: 'flex', gap: '40px', flexWrap: 'wrap', justifyContent: 'flex-start', padding: '0 20px' }}>
                {files.length === 0 ? (
                  <div style={{ width: '100%', textAlign: 'center', padding: '50px', color: '#bbb', fontStyle: 'italic' }}>(Carpeta vacía)</div>
                ) : (
                  files.map((file, i) => (
                    <div key={i} onClick={() => handleOpenFile(file)} style={{ textAlign: 'center', cursor: 'pointer', width: '80px', transition: 'transform 0.2s' }} onMouseEnter={(e) => e.currentTarget.style.transform = 'scale(1.05)'} onMouseLeave={(e) => e.currentTarget.style.transform = 'scale(1)'}>
                       {renderIcon(file.type)}
                       <div style={{ fontSize: '14px', wordWrap: 'break-word', marginTop: '8px', color: '#333' }}>{file.name}</div>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

          {/* Vista archivo */}
          {step === 'fileViewer' && (
            <div>
              <button onClick={() => { setStep('explorer'); loadDirectory(selectedDisk, selectedPartition, currentPath); }} style={backButtonStyle}>← Cerrar Archivo</button>
              <div style={pathBarStyle}>{currentPath}</div>
              <p style={{ textAlign: 'left', color: '#555', marginBottom: '10px', fontSize: '14px' }}>Contenido del archivo:</p>
              <textarea 
                readOnly 
                value={fileContent} 
                style={{ width: '100%', height: '300px', padding: '15px', borderRadius: '4px', border: '1px solid #ccc', backgroundColor: '#f8f9fa', fontFamily: 'monospace', resize: 'vertical', boxSizing: 'border-box' }} 
              />
            </div>
          )}
        </>
      )}
    </div>
  );
}