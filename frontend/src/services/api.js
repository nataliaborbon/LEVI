/**
 * @file api.js
 * @brief Cliente HTTP centralizado para comunicación con el backend ESP32.
 *
 * Todas las llamadas a la API pasan por aquí. La URL base apunta
 * directamente a la IP del CYD ya que la app corre desde LittleFS,
 * en la misma red WiFi local.
 */

const BASE_URL = '';

// ---------------------------------------------------------------------------
// HELPERS INTERNOS
// ---------------------------------------------------------------------------

/**
 * Devuelve el token de sesión normal (profesor/tutor/invitado).
 * Está en localStorage para que todas las pestañas compartan la sesión: si
 * se abre otra pestaña mientras la sesión sigue viva en la S3, entra directo.
 * Si la sesión ya venció, la S3 rechaza el token y se pide login.
 */
const getToken = () => localStorage.getItem('token');

/**
 * Devuelve el token temporal del alumno.
 */
const getAlumnoToken = () => sessionStorage.getItem('tokenAlumno');

/**
 * Genera un id aleatorio de 32 caracteres hexadecimales.
 * crypto.randomUUID no existe fuera de HTTPS; getRandomValues sí.
 */
const generarId = () => {
    const bytes = new Uint8Array(16);
    crypto.getRandomValues(bytes);
    return Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');
};

// ---------------------------------------------------------------------------
// UNA SOLA PESTAÑA POR SESIÓN
// ---------------------------------------------------------------------------
// La sesión se comparte entre pestañas, pero solo una puede usarla a la vez.
// Cada pestaña tiene su id (en memoria: si estuviera en sessionStorage,
// "Duplicar pestaña" lo copiaría) y lo manda en cada request. La S3 solo
// acepta los de la pestaña activa y a las demás les responde OTRA_PESTANA.
// La pestaña que entra a una pantalla toma el control (X-Tomar-Pestana) y
// avisa a las otras por BroadcastChannel para que se bloqueen enseguida.
//
// Hay dos ámbitos independientes: 'panel' (profesor, tutor e invitado
// comparten el token del panel) y 'alumno'.

const ID_PESTAÑA = generarId();

const pestañas = {
    panel: { quiereTomar: true, bloqueada: false },
    alumno: { quiereTomar: true, bloqueada: false },
};
let ambitoActual = null;
const oyentesPestaña = new Set();

let canalPestañas = null;
try {
    canalPestañas = new BroadcastChannel('levi-pestanas');
} catch (_) {
    /* sin soporte: avisa la S3 */
}

const avisarCambioPestaña = () => oyentesPestaña.forEach((cb) => cb());

const bloquearPestaña = (ambito) => {
    const estado = pestañas[ambito];
    if (estado.bloqueada) return;
    estado.bloqueada = true;
    estado.quiereTomar = false;
    avisarCambioPestaña();
};

if (canalPestañas) {
    canalPestañas.onmessage = (e) => {
        const { ambito, id } = e.data || {};
        if (!pestañas[ambito] || id === ID_PESTAÑA) return;
        // Si esta pestaña está usando ese ámbito, se bloquea. Si no, cuando
        // vuelva a entrar tiene que tomar el control de nuevo.
        if (ambito === ambitoActual) bloquearPestaña(ambito);
        else pestañas[ambito].quiereTomar = true;
    };
}

const errorOtraPestaña = () => {
    const error = new Error('La sesión está abierta en otra pestaña.');
    error.status = 409;
    error.codigo = 'OTRA_PESTANA';
    return error;
};

/**
 * Devuelve el ámbito de sesión de una ruta: 'panel', 'alumno' o null.
 */
export const ambitoDeRuta = (path) => {
    if (path.startsWith('/panel') || path === '/invitado') return 'panel';
    if (path === '/alumno') return 'alumno';
    return null;
};

/**
 * Avisa en qué ámbito está la pantalla actual. Entrar a un ámbito es "abrir"
 * la sesión en esta pestaña: si estaba bloqueada, vuelve a tomar el control.
 * @note No notifica a los oyentes: se llama mientras App se está dibujando.
 */
export const entrarAmbito = (ambito) => {
    if (ambito === ambitoActual) return;
    ambitoActual = ambito;
    if (ambito && pestañas[ambito].bloqueada) {
        pestañas[ambito].bloqueada = false;
        pestañas[ambito].quiereTomar = true;
    }
};

/**
 * Indica si otra pestaña tiene el control de la sesión de ese ámbito.
 */
export const pestañaBloqueada = (ambito) => !!pestañas[ambito]?.bloqueada;

/**
 * Hace que esta pestaña vuelva a tomar el control (botón "Usar acá").
 */
export const usarEstaPestaña = (ambito) => {
    pestañas[ambito].bloqueada = false;
    pestañas[ambito].quiereTomar = true;
    avisarCambioPestaña();
};

/**
 * Suscribe a los cambios de bloqueo de la pestaña.
 * @returns {Function} Función para desuscribirse.
 */
export const alCambiarPestaña = (cb) => {
    oyentesPestaña.add(cb);
    return () => oyentesPestaña.delete(cb);
};

/**
 * Agrega los headers de la pestaña. Mientras esta pestaña quiere tomar el
 * control de la sesión, se lo pide a la S3 en cada request.
 */
const agregarHeadersPestaña = (headers, ambito) => {
    headers['X-Pestana'] = ID_PESTAÑA;
    if (pestañas[ambito].quiereTomar) headers['X-Tomar-Pestana'] = '1';
    return headers;
};

/**
 * Construye los headers comunes para todos los requests.
 * Si hay token activo lo incluye como Bearer.
 */
const buildHeaders = () => {
    const headers = { 'Content-Type': 'application/json' };
    const token = getToken();
    if (token) headers['Authorization'] = `Bearer ${token}`;
    return agregarHeadersPestaña(headers, 'panel');
};

/**
 * Construye los headers para los endpoints del alumno,
 * usando el tokenAlumno en lugar del token de sesión normal.
 */
const buildAlumnoHeaders = () => {
    const headers = { 'Content-Type': 'application/json' };
    const token = getAlumnoToken();
    if (token) headers['Authorization'] = `Bearer ${token}`;
    return agregarHeadersPestaña(headers, 'alumno');
};

/**
 * Devuelve el identificador de este dispositivo para la sesión del alumno.
 * Se guarda en localStorage, así es el mismo en todas las pestañas y aunque
 * se reinicie la placa: si el alumno recarga o se reconecta, la S3 le
 * devuelve su sesión en vez de decir que hay otro alumno conectado.
 */
let idDispositivoEnMemoria = null;
const getIdDispositivo = () => {
    try {
        const guardado = localStorage.getItem('idDispositivo');
        if (guardado) return guardado;
    } catch (_) {
        /* sin localStorage: se usa el de memoria */
    }

    if (!idDispositivoEnMemoria) idDispositivoEnMemoria = generarId();
    try {
        localStorage.setItem('idDispositivo', idDispositivoEnMemoria);
    } catch (_) {}
    return idDispositivoEnMemoria;
};

/**
 * Ejecuta un fetch y devuelve el JSON parseado.
 * Lanza un Error con el mensaje del backend si el status no es 2xx.
 * El Error trae además:
 *   - status: código HTTP (0 si no hubo conexión).
 *   - codigo: código del backend (ej: 'SESION_CERRADA', 'ALUMNO_OCUPADO').
 *   - motivo: por qué se cerró la sesión, si codigo es 'SESION_CERRADA'.
 * Si otra pestaña tiene el control de la sesión, falla con codigo
 * 'OTRA_PESTANA' (sin llegar a la S3 si esta pestaña ya sabe que está bloqueada).
 * @param {string} method
 * @param {string} path
 * @param {object|null} body
 * @param {boolean} alumno - Si true, usa el tokenAlumno en lugar del token normal.
 */
const request = async (method, path, body = null, alumno = false) => {
    const ambito = alumno ? 'alumno' : 'panel';
    const estadoPestaña = pestañas[ambito];
    if (estadoPestaña.bloqueada) throw errorOtraPestaña();

    const options = {
        method,
        headers: alumno ? buildAlumnoHeaders() : buildHeaders(),
    };
    if (body) options.body = JSON.stringify(body);
    const pedíaTomar = !!options.headers['X-Tomar-Pestana'];
    const conToken = !!options.headers['Authorization'];

    let response;
    try {
        response = await fetch(`${BASE_URL}${path}`, options);
    } catch (_) {
        // La placa no respondió (se cortó la conexión, se reinició, sin WiFi, etc.)
        const error = new Error('No se pudo conectar con LEVI. Revisá la conexión e intentá de nuevo.');
        error.status = 0;
        error.codigo = 'SIN_CONEXION';
        throw error;
    }

    // Si la respuesta no es JSON (por ejemplo, un error de texto plano), no se
    // muestra el error técnico del parser sino el texto o un mensaje genérico.
    const texto = await response.text();
    let data = {};
    try {
        data = texto ? JSON.parse(texto) : {};
    } catch (_) {
        data = { mensaje: texto };
    }

    if (!response.ok) {
        const error = new Error(data.mensaje || 'Error desconocido del servidor.');
        error.status = response.status;
        error.codigo = data.codigo || '';
        error.motivo = data.motivo || '';
        if (error.codigo === 'OTRA_PESTANA') bloquearPestaña(ambito);
        throw error;
    }

    // La S3 le dio el control a esta pestaña (con un token válido, o al
    // iniciar sesión): se deja de pedir y se avisa a las demás. Si mientras
    // tanto otra pestaña tomó el control, esta no la pisa.
    if (pedíaTomar && (conToken || data.token) && !estadoPestaña.bloqueada) {
        estadoPestaña.quiereTomar = false;
        canalPestañas?.postMessage({ ambito, id: ID_PESTAÑA });
    }

    return data;
};

// ---------------------------------------------------------------------------
// AUTH
// ---------------------------------------------------------------------------

/**
 * Inicia sesión con usuario y contraseña.
 * Guarda el token y datos de sesión en localStorage (compartido entre pestañas).
 * @returns {{ token, rol, idUsuario, nombre }}
 */
export const login = async (usuario, password) => {
    const data = await request('POST', '/api/auth/login', { usuario, password });
    localStorage.setItem('token', data.token);
    localStorage.setItem('rol', data.rol);
    localStorage.setItem('idUsuario', data.idUsuario);
    localStorage.setItem('nombre', data.nombre);
    return data;
};

/**
 * Inicia sesión como invitado.
 * Guarda el token en localStorage (compartido entre pestañas).
 * @returns {{ token, rol }}
 */
export const loginInvitado = async () => {
    const data = await request('POST', '/api/auth/invitado');
    localStorage.setItem('token', data.token);
    localStorage.setItem('rol', data.rol);
    return data;
};

/**
 * Borra del localStorage los datos de la sesión del panel.
 * No toca el token del alumno ni la calibración guardada.
 */
export const limpiarSesion = () => {
    ['token', 'rol', 'idUsuario', 'nombre', 'nombreSesionActiva'].forEach((clave) => localStorage.removeItem(clave));
};

/**
 * Indica si hay una sesión de profesor o tutor guardada en este navegador.
 */
export const haySesionPanel = () => {
    const rol = localStorage.getItem('rol');
    return !!getToken() && (rol === 'profesor' || rol === 'tutor');
};

/**
 * Texto para avisarle al usuario por qué se cerró su sesión.
 * @param {Error} error - Error con codigo 'SESION_CERRADA' (ver request()).
 */
export const mensajeSesionCerrada = (error) => {
    const mensajes = {
        inactividad: 'Se cerró la sesión por inactividad.',
        desconexion: 'Se cerró la sesión porque se perdió la conexión con LEVI.',
        expulsado: 'Se cerró tu sesión porque entró un usuario con mayor prioridad.',
        reemplazada: 'Se cerró esta sesión porque iniciaste sesión en otro lugar.',
        logout: 'Se cerró la sesión desde otra pestaña.',
    };
    return mensajes[error?.motivo] || 'Tu sesión expiró. Volvé a iniciar sesión.';
};

/**
 * Cierra la sesión activa y limpia localStorage.
 */
export const logout = async () => {
    try {
        await request('POST', '/api/auth/logout');
    } catch (e) {
        // Una pestaña sin el control no cierra la sesión que usa la otra.
        if (e.codigo === 'OTRA_PESTANA') return;
        // Si el servidor ya cerró la sesión igual limpiamos el cliente
    }
    limpiarSesion();
};

/**
 * Obtiene los datos del perfil del usuario logueado.
 * @returns {Usuario}
 */
export const getPerfil = () => request('GET', '/api/auth/perfil');

/**
 * Mantiene viva la sesión panel. Llamar cada 10 segundos.
 * @param {boolean} interaccion - true si el usuario usó la pantalla desde el
 *                                heartbeat anterior (renueva la inactividad).
 */
export const heartbeat = (interaccion) => request('POST', `/api/auth/heartbeat?interaccion=${interaccion ? 1 : 0}`);

// ---------------------------------------------------------------------------
// USUARIOS
// ---------------------------------------------------------------------------

/**
 * Crea un usuario nuevo (con programación defensiva).
 * @param {object} datos
 */
export const crearUsuario = async (datos) => {
    const response = await fetch(`${BASE_URL}/api/usuarios`, {
        method: 'POST',
        headers: buildHeaders(),
        body: JSON.stringify(datos),
    });

    const textoResp = await response.text();
    let data = {};

    try {
        data = textoResp ? JSON.parse(textoResp) : {};
    } catch (e) {
        data = { mensaje: textoResp };
    }

    if (!response.ok) {
        throw new Error(data.mensaje || 'Error al crear el usuario.');
    }

    return data;
};

/**
 * Elimina un usuario por nombre de login.
 * @param {string} nombreUsuario
 * @param {string} claveMaestra
 */
export const eliminarUsuario = (nombreUsuario, claveMaestra) =>
    request('DELETE', `/api/usuarios/${nombreUsuario}`, { claveMaestra });

/**
 * Edita el perfil del usuario logueado.
 * @param {{ usuario, nombre, apellido, materia, contacto }} datos
 */
export const editarPerfil = (datos) => request('PUT', '/api/usuarios/perfil', datos);

/**
 * Recupera la contraseña desde el login (requiere clave maestra y usuario).
 * @param {{ usuario, claveMaestra, passwordNueva, confirmar }} datos
 */
export const recuperarPassword = (datos) => request('PUT', '/api/usuarios/password', datos);

/**
 * Cambia la contraseña del usuario logueado desde su perfil.
 * @param {{ passwordActual, passwordNueva, confirmar }} datos
 */
export const cambiarMiPassword = (datos) => request('PUT', '/api/usuarios/perfil/password', datos);

/**
 * Lista una página de profesores (para panel tutor y profesor).
 * @param {number} pagina Número de página (empieza en 1).
 * @returns {{ profesores: ProfesorResumen[], pagina, porPagina, total, totalPaginas }}
 */
export const getProfesores = (pagina = 1) => request('GET', `/api/usuarios/profesores?pagina=${pagina}`);

/**
 * Lista una página de tutores (para panel profesor).
 * @param {number} pagina Número de página (empieza en 1).
 * @returns {{ tutores: TutorResumen[], pagina, porPagina, total, totalPaginas }}
 */
export const getTutores = (pagina = 1) => request('GET', `/api/usuarios/tutores?pagina=${pagina}`);

/**
 * Elimina la cuenta del usuario actualmente logueado (con programación defensiva).
 * @param {number} id
 */
export const eliminarUsuarioPorId = async (id) => {
    const response = await fetch(`${BASE_URL}/api/usuario?id=${id}`, {
        method: 'DELETE',
        headers: buildHeaders(),
    });
    const textoResp = await response.text();
    let data = {};

    try {
        data = textoResp ? JSON.parse(textoResp) : {};
    } catch (e) {
        data = { mensaje: textoResp };
    }

    if (!response.ok) {
        throw new Error(data.mensaje || 'Error al intentar eliminar la cuenta.');
    }

    return data;
};

// ---------------------------------------------------------------------------
// ALUMNO / EXAMEN
// ---------------------------------------------------------------------------

/**
 * Envía una pregunta rápida como usuario invitado.
 * @param {{ pregunta, opciones }} datos
 */
export const enviarPreguntaInvitado = (datos) => request('POST', '/api/invitado/pregunta', datos);

/**
 * Inicia la sesión del alumno y obtiene un token temporal.
 * Este endpoint NO requiere token previo; pasa alumno=true para que cuente
 * como ámbito alumno (esta pestaña toma el control de la sesión del alumno).
 * Si este dispositivo ya tenía la sesión, devuelve la misma.
 */
export const alumnoIniciar = () => request('POST', '/api/alumno/iniciar', { idDispositivo: getIdDispositivo() }, true);

/**
 * Libera la sesión del alumno y borra su token.
 * Usa keepalive para que el request salga aunque se esté cerrando la pestaña.
 */
export const alumnoSalir = () => {
    const token = getAlumnoToken();
    sessionStorage.removeItem('tokenAlumno');
    // Si otra pestaña tiene el control, la sesión es de ella: no se cierra.
    if (!token || pestañas.alumno.bloqueada) return;
    fetch(`${BASE_URL}/api/alumno/salir`, {
        method: 'POST',
        headers: { Authorization: `Bearer ${token}`, 'X-Pestana': ID_PESTAÑA },
        keepalive: true,
    }).catch(() => {
        /* si no llega, la sesión vence sola */
    });
};

/**
 * Mantiene viva la conexión del alumno.
 * Usa el tokenAlumno.
 */
export const alumnoHeartbeat = () => request('POST', '/api/alumno/heartbeat', {}, true);

/**
 * Consulta el estado actual del examen y la pregunta activa.
 * Usa el tokenAlumno.
 */
export const alumnoEstado = () => request('GET', '/api/alumno/estado', null, true);

/**
 * Envía la respuesta del alumno a una pregunta.
 * Usa el tokenAlumno.
 * @param {number} idPregunta
 * @param {number} idOpcion
 */
export const alumnoResponder = (idPregunta, idOpcion) =>
    request('POST', '/api/alumno/responder', { idPregunta, idOpcion }, true);

// ---------------------------------------------------------------------------
// CUESTIONARIOS
// ---------------------------------------------------------------------------

/**
 * Obtiene una página de la lista de cuestionarios para el rol actual.
 * @param {number} pagina Número de página (empieza en 1).
 * @returns {{ cuestionarios: [], pagina, porPagina, total, totalPaginas }}
 */
export const getCuestionarios = (pagina = 1) => request('GET', `/api/cuestionarios?pagina=${pagina}`);

/**
 * Cambia el estado de un cuestionario (iniciar, pausar, reanudar, finalizar).
 * @param {number} id
 * @param {string} accion (e.g. 'iniciar')
 * @param {object} payload
 */
export const cambiarEstadoCuestionario = (id, accion, payload = {}) =>
    request('PATCH', `/api/cuestionario/${accion}?id=${id}`, payload);

/**
 * Elimina un cuestionario por ID (con programación defensiva).
 * @param {number} id
 */
export const eliminarCuestionarioPorId = async (id) => {
    const response = await fetch(`${BASE_URL}/api/cuestionario?id=${id}`, {
        method: 'DELETE',
        headers: buildHeaders(),
    });

    const textoResp = await response.text();
    let data = {};

    try {
        data = textoResp ? JSON.parse(textoResp) : {};
    } catch (e) {
        data = { mensaje: textoResp };
    }

    if (!response.ok) {
        throw new Error(data.mensaje || 'Error al eliminar el cuestionario.');
    }

    return data;
};

/**
 * Obtiene la revisión de un cuestionario finalizado.
 * @param {number} id
 */
export const getRevisionCuestionario = (id) => request('GET', `/api/cuestionario/revision?id=${id}`);

/**
 * Obtiene un cuestionario completo con sus preguntas y opciones
 */
export async function getCuestionarioCompleto(id) {
    return request('GET', `/api/cuestionario?id=${id}`);
}

/**
 * Edita un cuestionario existente
 */
export async function editarCuestionario(id, datos) {
    return request('PUT', `/api/cuestionario?id=${id}`, datos);
}

/**
 * Crea un cuestionario nuevo (POST)
 */
export async function crearCuestionario(datos) {
    return request('POST', '/api/cuestionarios', datos);
}
