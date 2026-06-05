#include <stdint.h>
namespace net::http
{
enum HTTPStatus : uint16_t
{
    OK = 200,
    Created = 201,
    NoContent = 204,
    BadRequest = 400,
    NotFound = 404,
    InternalServerError = 500
};

} // namespace net::http