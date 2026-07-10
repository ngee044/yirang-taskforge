// 세 인터페이스가 공유하는 응답 envelope {success, data, error}
export interface ErrorBody {
  code: string;
  message: string;
}

export interface Envelope<T> {
  success: boolean;
  data?: T;
  error?: ErrorBody;
}

export function successEnvelope<T>(data: T): Envelope<T> {
  return { success: true, data };
}

export function errorEnvelope(code: string, message: string): Envelope<never> {
  return { success: false, error: { code, message } };
}
