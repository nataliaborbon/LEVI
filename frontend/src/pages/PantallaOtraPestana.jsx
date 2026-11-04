/**
 * @file PantallaOtraPestana.jsx
 * @brief Se muestra cuando la sesión se está usando desde otra pestaña.
 *
 * Solo una pestaña puede usar la sesión a la vez. Esta deja de pedir datos
 * (la pantalla real se desmonta) hasta que el usuario decida usarla acá.
 */

import './PantallaHome.css';

function PantallaOtraPestana({ onUsarAca }) {
    return (
        <div className="home-page-container">
            <h1 className="home-welcome">LEVI está abierto en otra pestaña</h1>

            <div className="home-buttons-group">
                <button className="btn-azul btn-home-large" onClick={onUsarAca}>
                    Usar acá
                </button>
            </div>

            <div className="home-footer">
                Solo se puede usar una pestaña a la vez. Si usás esta, la otra se bloquea.
            </div>
        </div>
    );
}

export default PantallaOtraPestana;
