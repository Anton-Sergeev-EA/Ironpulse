// Asks for the API token when the server requires one (api_token in the
// engine's configuration). The token is checked against the API before
// it is accepted, then remembered by IronpulseApi.
const AuthDialog = {
    dialog: null,
    pending: null,

    init() {
        this.dialog = document.getElementById("auth-dialog");
        const form = document.getElementById("auth-form");
        form.addEventListener("submit", (event) => {
            event.preventDefault();
            this._submit();
        });
        // Escape would close the dialog with nothing behind it to show.
        this.dialog.addEventListener("cancel", (event) => event.preventDefault());
    },

    // Resolves once a working token has been entered.
    ask({ invalid = false } = {}) {
        if (!this.pending) {
            this.pending = new Promise((resolve) => {
                this._resolve = resolve;
            });
            document.getElementById("auth-error").hidden = !invalid;
            document.getElementById("auth-token").value = "";
            this.dialog.showModal();
            document.getElementById("auth-token").focus();
        }
        return this.pending;
    },

    async _submit() {
        const input = document.getElementById("auth-token");
        const button = document.getElementById("auth-submit");
        const token = input.value.trim();
        if (!token) {
            input.focus();
            return;
        }
        button.disabled = true;
        const previous = IronpulseApi.token;
        IronpulseApi.saveToken(token);
        try {
            await IronpulseApi.getSensors();
            this.dialog.close();
            const resolve = this._resolve;
            this.pending = null;
            resolve();
        } catch (err) {
            IronpulseApi.saveToken(previous);
            const error = document.getElementById("auth-error");
            error.dataset.i18n = err instanceof UnauthorizedError ? "auth.invalid" : "auth.unreachable";
            error.textContent = I18n.t(error.dataset.i18n);
            error.hidden = false;
            input.select();
        } finally {
            button.disabled = false;
        }
    },
};

document.addEventListener("DOMContentLoaded", () => {
    AuthDialog.init();
});
