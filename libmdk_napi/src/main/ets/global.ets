import nativeModule from 'libmdk_napi.so';

export const TimestampEOS: number = Number.MAX_VALUE;
export const TimeScaleForInt: number = 1000.0;
export const IgnoreAspectRatio: number = 0.0;
export const KeepAspectRatio: number = Number.EPSILON;
export const KeepAspectRatioCrop: number = -Number.EPSILON;

export function version(): number {
  return nativeModule.version();
}

export function setGlobalOptionString(key: string, value: string): void {
  nativeModule.setGlobalOptionString(key, value);
}

export function getGlobalOptionString(key: string): string | null {
  return nativeModule.getGlobalOptionString(key);
}

export function setGlobalOptionInt(key: string, value: number): void {
  nativeModule.setGlobalOptionInt(key, value);
}

export function getGlobalOptionInt(key: string): number | null {
  return nativeModule.getGlobalOptionInt(key);
}

export function setGlobalOptionFloat(key: string, value: number): void {
  nativeModule.setGlobalOptionFloat(key, value);
}

