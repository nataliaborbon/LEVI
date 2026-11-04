/**
 * @file PantallaPanel.jsx
 * @brief Menú principal unificado para profesor y tutor.
 *
 * Discrimina por `rol` (obtenido de localStorage) qué opciones mostrar:
 *   - profesor: Cuestionarios / Ver datos tutor / Editar perfil
 *   - tutor:    Cuestionarios / Ver profesores  / Editar perfil
 */

import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import '../panel.css';
import { getPerfil } from '../../services/api';

function PantallaPanel({ onLogout }) {
    const navigate = useNavigate();
    const rol = localStorage.getItem('rol');
    // Se arranca con el nombre guardado al loguearse, así no aparece
    // "Profesor" un instante mientras llega el perfil.
    const [nombreUsuario, setNombreUsuario] = useState(
        localStorage.getItem('nombre') || (rol === 'tutor' ? 'Tutor' : 'Profesor'),
    );

    useEffect(() => {
        const cargarNombre = async () => {
            try {
                const data = await getPerfil();
                const nombreCompleto = `${data.nombre} ${data.apellido}`;
                setNombreUsuario(nombreCompleto);
                localStorage.setItem('nombre', nombreCompleto);
            } catch (_) {}
        };
        cargarNombre();
    }, []);

    const handleLogout = () => {
        if (window.confirm('¿Cerrar sesión?')) onLogout();
    };

    const botonCentral =
        rol === 'tutor'
            ? { texto: 'Ver profesores', clase: 'color-contactos', ruta: '/panel/contactos' }
            : { texto: 'Ver tutores', clase: 'color-tutor', ruta: '/panel/contactos' };

    return (
        <div className="panel-menu-container">
            <div className="panel-header">
                <h2>Hola, {nombreUsuario}</h2>
                <button className="btn-rojo-logout" onClick={handleLogout}>
                    Cerrar sesión
                </button>
            </div>

            <div className="panel-botones-centrales">
                <button className="btn-menu-largo color-cuestionarios" onClick={() => navigate('/panel/cuestionarios')}>
                    Ver cuestionarios
                </button>

                <button className={`btn-menu-largo ${botonCentral.clase}`} onClick={() => navigate(botonCentral.ruta)}>
                    {botonCentral.texto}
                </button>

                <button className="btn-menu-largo color-perfil" onClick={() => navigate('/panel/perfil')}>
                    Editar perfil
                </button>
            </div>
        </div>
    );
}

export default PantallaPanel;
