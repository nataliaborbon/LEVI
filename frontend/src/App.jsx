import { Routes, Route, Navigate, useNavigate, useLocation } from 'react-router-dom';
import { useEffect, useReducer } from 'react';
import {
    logout,
    heartbeat,
    limpiarSesion,
    mensajeSesionCerrada,
    haySesionPanel,
    ambitoDeRuta,
    entrarAmbito,
    pestañaBloqueada,
    usarEstaPestaña,
    alCambiarPestaña,
} from './services/api';
import PantallaHome from './pages/PantallaHome';
import PantallaLogueo from './pages/PantallaLogueo';
import PantallaPanel from './pages/panel/PantallaPanel';
import PantallaCuestionario from './pages/panel/PantallaCuestionario';
import PantallaPerfil from './pages/panel/PantallaPerfil';
import PantallaContactos from './pages/panel/PantallaContactos';
import PantallaInvitado from './pages/PantallaInvitado';
import PantallaAlumno from './pages/PantallaAlumno';
import PantallaOtraPestana from './pages/PantallaOtraPestana';

/**
 * Deja pasar al panel solo si hay una sesión de profesor o tutor.
 * Si la sesión ya venció en la S3, el monitor de sesión lo detecta en el
 * primer heartbeat y manda al login.
 */
function RutaPanel({ children }) {
    return haySesionPanel() ? children : <Navigate to="/login" replace />;
}

function App() {
    const navigate = useNavigate();
    const location = useLocation();

    // -----------------------------------------------------------------------
    // UNA SOLA PESTAÑA POR SESIÓN
    // -----------------------------------------------------------------------
    // Entrar a una pantalla con sesión (panel, invitado o alumno) toma el
    // control en esta pestaña. Si otra pestaña lo toma, esta muestra el aviso
    // en lugar de la pantalla, así deja de pedir datos, de latir y de usar la
    // cámara.
    const [, redibujar] = useReducer((n) => n + 1, 0);
    useEffect(() => alCambiarPestaña(redibujar), []);

    // entrarAmbito solo hace algo cuando cambia el ámbito de la ruta.
    const ambito = ambitoDeRuta(location.pathname);
    entrarAmbito(ambito);
    const bloqueada = !!ambito && pestañaBloqueada(ambito);

    const handleLogout = async () => {
        await logout();
        navigate('/login');
    };

    // -----------------------------------------------------------------------
    // MONITOR GLOBAL DE SESIÓN (panel e invitado)
    // -----------------------------------------------------------------------
    // La S3 decide cuándo vence la sesión (por desconexión o por inactividad).
    // Acá solo se manda el heartbeat avisando si el usuario usó la pantalla,
    // y si la S3 responde que la sesión se cerró se avisa el motivo.
    useEffect(
        function monitorSesion() {
            if (bloqueada) return;
            const enPanel = location.pathname.startsWith('/panel');
            const enInvitado = location.pathname === '/invitado';
            if (!enPanel && !enInvitado) return;

            // Cambiar de pantalla ya es usar la app.
            let huboInteraccion = true;
            let cerrada = false;
            const marcarInteraccion = () => {
                huboInteraccion = true;
            };
            const eventos = ['pointerdown', 'mousemove', 'keydown', 'touchstart', 'scroll'];
            eventos.forEach((e) => window.addEventListener(e, marcarInteraccion, { passive: true, capture: true }));

            const latir = async () => {
                // En /invitado el token aparece recién cuando la pantalla termina de entrar.
                if (cerrada || !localStorage.getItem('token')) return;

                const interaccion = huboInteraccion;
                huboInteraccion = false;

                try {
                    await heartbeat(interaccion);
                } catch (e) {
                    if (e.codigo !== 'SESION_CERRADA' || cerrada) return; // sin conexión: se reintenta
                    cerrada = true;
                    limpiarSesion();
                    alert(mensajeSesionCerrada(e));
                    navigate('/login');
                }
            };

            const alVolverALaPestaña = () => {
                if (document.visibilityState === 'visible') latir();
            };
            document.addEventListener('visibilitychange', alVolverALaPestaña);

            // La sesión se comparte entre pestañas (localStorage). Si otra pestaña
            // cierra sesión, esta va al login; si otra inicia sesión con otro
            // usuario, esta se recarga para tomar la sesión nueva.
            const alCambiarEnOtraPestaña = (e) => {
                if (e.key !== 'token' || cerrada) return;
                cerrada = true;
                if (!e.newValue) navigate('/login');
                else window.location.reload();
            };
            window.addEventListener('storage', alCambiarEnOtraPestaña);

            latir();
            const intervalo = setInterval(latir, 10000);

            return () => {
                eventos.forEach((e) => window.removeEventListener(e, marcarInteraccion, { capture: true }));
                document.removeEventListener('visibilitychange', alVolverALaPestaña);
                window.removeEventListener('storage', alCambiarEnOtraPestaña);
                clearInterval(intervalo);
            };
        },
        [location.pathname, bloqueada],
    );

    // -----------------------------------------------------------------------
    // FUNCIONES DE NAVEGACIÓN
    // -----------------------------------------------------------------------

    const handleLoginSuccess = (rol) => {
        if (rol === 'invitado') navigate('/invitado');
        else navigate('/panel');
    };

    const handleEntrarAlumno = () => {
        navigate('/alumno');
    };

    // -----------------------------------------------------------------------
    // RUTAS
    // -----------------------------------------------------------------------

    if (bloqueada) {
        return <PantallaOtraPestana onUsarAca={() => usarEstaPestaña(ambito)} />;
    }

    return (
        <Routes>
            {/* Home */}
            <Route
                path="/"
                element={
                    <PantallaHome onEntrarAlumno={handleEntrarAlumno} onEntrarUsuario={() => navigate('/login')} />
                }
            />

            {/* Login: si ya hay sesión en esta pestaña, sigue en el panel */}
            <Route
                path="/login"
                element={
                    haySesionPanel() ? (
                        <Navigate to="/panel" replace />
                    ) : (
                        <PantallaLogueo onLoginSuccess={handleLoginSuccess} onGoBack={() => navigate('/')} />
                    )
                }
            />

            {/* ── Panel unificado (profesor + tutor) ── */}
            <Route
                path="/panel"
                element={
                    <RutaPanel>
                        <PantallaPanel onLogout={handleLogout} />
                    </RutaPanel>
                }
            />
            <Route
                path="/panel/cuestionarios"
                element={
                    <RutaPanel>
                        <PantallaCuestionario />
                    </RutaPanel>
                }
            />
            <Route
                path="/panel/perfil"
                element={
                    <RutaPanel>
                        <PantallaPerfil onLogout={handleLogout} />
                    </RutaPanel>
                }
            />
            <Route
                path="/panel/contactos"
                element={
                    <RutaPanel>
                        <PantallaContactos />
                    </RutaPanel>
                }
            />

            {/* Invitado y Alumno */}
            <Route path="/invitado" element={<PantallaInvitado onLogout={() => navigate('/login')} />} />
            <Route
                path="/alumno"
                element={<PantallaAlumno onLogout={() => navigate('/')} onAlumnoOcupado={() => navigate('/')} />}
            />

            {/* Catch-all */}
            <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
    );
}

export default App;
