#pragma once

#include "models.h"

enum group_visibility {
    GROUP_PUBLIC = 0,
    GROUP_PRIVATE = 1
};

/* Creează un grup, îl setează public/privat și îl face pe creator admin + membru.
 * out_conversation_id (opțional, poate fi NULL) întoarce id-ul conversației.
 * Return:
 *   0  = succes
 *  -1  = eroare SQL
 *  -2  = există deja un grup cu acest nume
 */
int group_create(const char *name, int creator_id,
                 enum group_visibility vis,
                 int *out_conversation_id);

/* JOIN_GROUP:
 *  - dacă grupul e PUBLIC: intră direct ca membru normal
 *  - dacă e PRIVATE: se creează cerere de join (pending)
 * Return:
 *   0  = succes (joined sau request created)
 *  -1  = eroare SQL
 *  -2  = grupul nu există
 *  -3  = deja membru
 */
int group_join(const char *name, int user_id);

/* Explicit: trimite cerere de join la un grup PRIVATE.
 * Return:
 *   0  = succes
 *  -1  = eroare SQL
 *  -2  = grupul nu există
 *  -3  = grupul nu este privat
 *  -4  = deja membru
 *  -5  = cerere deja existentă
 */
int group_request_join(const char *name, int user_id);

/* Adminul acceptă un membru într-un grup (grup privat).
 * admin_id trebuie să fie admin al grupului.
 * Return:
 *   0  = succes
 *  -1  = eroare SQL
 *  -2  = grupul nu există
 *  -3  = admin_id nu este admin în grup
 *  -4  = nu există cerere de la user-ul respectiv
 */
int group_approve_member(const char *group_name, int admin_id,
                         const char *username);

/* Trimite un mesaj într-un grup.
 *  - verifică dacă sender e membru
 *  - inserează mesajul în DB
 *  - notifică TOȚI membrii online (ca la DM-uri)
 *
 * Return:
 *   0  = succes
 *  -1  = eroare SQL
 *  -2  = grupul nu există
 *  -3  = sender nu este membru al grupului
 */
int group_send_message(const char *group_name, int sender_id,
                       const char *content);
