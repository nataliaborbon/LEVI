/**
 * @file PantallaInvitado.jsx
 * @brief Pantalla para usuarios invitados (sin cuenta) que quieren enviar preguntas rápidas a un alumno.
 */

import { useState, useRef, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import './PantallaInvitado.css';
import {
    loginInvitado,
    enviarPreguntaInvitado,
    heartbeat,
    logout,
    limpiarSesion,
    mensajeSesionCerrada,
    haySesionPanel,
} from '../services/api';

function PantallaInvitado({ onLogout }) {
    const navigate = useNavigate();
    const [pregunta, setPregunta] = useState('');
    const [opciones, setOpciones] = useState(['', '', '', '']);
    // 'conectando' | 'lista' | 'error'
    const [estadoSesion, setEstadoSesion] = useState('conectando');
    const [errorSesion, setErrorSesion] = useState('');
    const [intento, setIntento] = useState(0);

    const refOp0 = useRef(null);
    const refOp1 = useRef(null);
    const refOp2 = useRef(null);
    const refOp3 = useRef(null);

    const arrayRefs = [refOp0, refOp1, refOp2, refOp3];

    // -----------------------------------------------------------------------
    // INICIO DE SESIÓN DE INVITADO
    // -----------------------------------------------------------------------
    useEffect(() => {
        // Un profesor o tutor logueado sigue en su panel.
        if (haySesionPanel()) {
            navigate('/panel', { replace: true });
            return;
        }

        let activo = true;

        const iniciarSesionInvitado = async () => {
            // Si ya había una sesión de invitado en esta pestaña (por ejemplo,
            // recargó la página), se valida antes de usarla.
            if (localStorage.getItem('token') && localStorage.getItem('rol') === 'invitado') {
                try {
                    await heartbeat(true);
                    if (activo) setEstadoSesion('lista');
                    return;
                } catch (e) {
                    if (e.codigo !== 'SESION_CERRADA') {
                        if (activo) {
                            setErrorSesion(e.message);
                            setEstadoSesion('error');
                        }
                        return;
                    }
                    limpiarSesion();
                }
            }

            try {
                await loginInvitado();
                if (activo) setEstadoSesion('lista');
            } catch (e) {
                console.error('iniciarSesionInvitado:', e);
                if (activo) {
                    setErrorSesion(e.message || 'Error al iniciar sesión de invitado.');
                    setEstadoSesion('error');
                }
            }
        };
        iniciarSesionInvitado();

        // Al salir de la pantalla (VOLVER, botón atrás, etc.) se libera la sesión.
        return () => {
            activo = false;
            if (localStorage.getItem('rol') === 'invitado') logout();
        };
    }, [intento]);

    // -----------------------------------------------------------------------
    // FUNCIONES DE MANEJO DE OPCIONES
    // -----------------------------------------------------------------------

    const actualizarOpcion = (index, valor) => {
        const nuevasOpciones = [...opciones];
        nuevasOpciones[index] = valor;
        setOpciones(nuevasOpciones);
    };

    const agregarOpcion = () => {
        if (opciones.length < 4) {
            setOpciones([...opciones, '']);
        }
    };

    const eliminarOpcion = (index) => {
        const nuevasOpciones = [...opciones];
        nuevasOpciones.splice(index, 1);
        setOpciones(nuevasOpciones);
    };

    // -----------------------------------------------------------------------
    // ENVIAR PREGUNTA AL SERVIDOR
    // -----------------------------------------------------------------------
    const guardarPregunta = async () => {
        if (!pregunta.trim()) {
            alert('Por favor, escribe una pregunta.');
            return;
        }

        const opcionesValidas = opciones.filter((opcion) => opcion.trim() !== '');

        if (opcionesValidas.length < 2) {
            alert('Por favor, ingresa al menos 2 opciones válidas.');
            return;
        }

        try {
            await enviarPreguntaInvitado({
                pregunta: pregunta.trim(),
                opciones: opcionesValidas,
            });

            alert('¡Pregunta enviada! Se va a mostrar en la pantalla del alumno.');
            setPregunta('');
            setOpciones(['', '', '', '']);
        } catch (e) {
            console.error('guardarPregunta:', e);
            if (e.codigo === 'SESION_CERRADA') {
                limpiarSesion();
                alert(mensajeSesionCerrada(e));
                onLogout();
                return;
            }
            alert(e.message || 'Error de conexión al guardar la pregunta.');
        }
    };

    // -----------------------------------------------------------------------
    // SESIÓN TODAVÍA NO LISTA
    // -----------------------------------------------------------------------

    if (estadoSesion !== 'lista') {
        return (
            <div className="invitado-page-container">
                <h1 className="invitado-titulo">Modo Invitado</h1>
                <p className="invitado-desc">
                    {estadoSesion === 'conectando' ? 'Conectando con LEVI...' : errorSesion}
                </p>
                {estadoSesion === 'error' && (
                    <div className="botones-finales-inv">
                        <button
                            className="btn-verde"
                            onClick={() => {
                                setEstadoSesion('conectando');
                                setIntento((i) => i + 1);
                            }}
                            style={{ padding: '15px 30px' }}
                        >
                            REINTENTAR
                        </button>
                        <button className="btn-gris" onClick={onLogout} style={{ padding: '15px 30px' }}>
                            VOLVER
                        </button>
                    </div>
                )}
            </div>
        );
    }

    // -----------------------------------------------------------------------
    // RENDERIZADO DE PANTALLA INVITADO
    // -----------------------------------------------------------------------

    return (
        <div className="invitado-page-container">
            <h1 className="invitado-titulo">Modo Invitado</h1>
            <p className="invitado-desc">Crea una pregunta rápida para tu amigo o familiar.</p>

            <div className="form-invitado">
                <h3 className="invitado-subtitulo">Tu Pregunta Personal</h3>

                <input
                    type="text"
                    className="pregunta-input-inv"
                    placeholder="Escribe la pregunta aquí..."
                    value={pregunta}
                    onChange={(e) => setPregunta(e.target.value)}
                    onKeyDown={(e) => {
                        if (e.key === 'Enter') {
                            e.preventDefault();
                            arrayRefs[0].current?.focus();
                        }
                    }}
                />

                <div className="opciones-container">
                    {opciones.map((opcion, index) => (
                        <div
                            key={index}
                            style={{ display: 'flex', gap: '10px', marginBottom: '10px', alignItems: 'center' }}
                        >
                            <input
                                type="text"
                                className="opcion-input-inv"
                                placeholder={`Opción ${index + 1}`}
                                value={opcion}
                                onChange={(e) => actualizarOpcion(index, e.target.value)}
                                ref={arrayRefs[index]}
                                onKeyDown={(e) => {
                                    if (e.key === 'Enter') {
                                        e.preventDefault();
                                        if (index < opciones.length - 1) {
                                            arrayRefs[index + 1].current?.focus();
                                        } else {
                                            guardarPregunta();
                                        }
                                    }
                                }}
                                style={{ flex: 1, margin: 0 }}
                            />
                            {opciones.length > 2 && (
                                <button
                                    className="btn-rojo"
                                    onClick={() => eliminarOpcion(index)}
                                    style={{ padding: '12px 15px', margin: 0, flexShrink: 0 }}
                                >
                                    X
                                </button>
                            )}
                        </div>
                    ))}

                    {opciones.length < 4 && (
                        <button
                            className="btn-gris"
                            onClick={agregarOpcion}
                            style={{ width: '100%', marginTop: '5px', padding: '10px', background: '#333' }}
                        >
                            + Agregar otra opción
                        </button>
                    )}
                </div>
            </div>

            <div className="botones-finales-inv">
                <button className="btn-verde" onClick={guardarPregunta} style={{ padding: '15px 30px' }}>
                    GUARDAR
                </button>
                <button className="btn-gris" onClick={onLogout} style={{ padding: '15px 30px' }}>
                    VOLVER
                </button>
            </div>
        </div>
    );
}

export default PantallaInvitado;
