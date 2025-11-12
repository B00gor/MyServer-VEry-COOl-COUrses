import http from 'k6/http';
import { check, sleep } from 'k6';

export let options = {
    stages: [
        { duration: '5s', target: 1000 },  // разогрев за 30 сек до 20 VU
        { duration: '1m', target: 1000 },   // держим 50 VU 1 минуту
        { duration: '20s', target: 0 },   // плавно останавливаемся
    ],
};

export default function () {
    let res = http.get('http://localhost:5555/users');

    check(res, {
        'is status 200': (r) => r.status === 200,
    });
}