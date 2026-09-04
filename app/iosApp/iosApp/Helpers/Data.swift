//
//  Data.swift
//  iosApp
//
//  Created by Tom Arlt on 28.08.26.
//

import Foundation

extension Data{
    var asString: String {
        String(decoding: self, as: UTF8.self)
    }
    
    func asJson<T>() -> T? {
        return try? JSONSerialization.jsonObject(with: self, options: []) as? T
    }

    func asJson<T: Decodable>() -> T? {
        do {
            return try JSONDecoder().decode(T.self, from: self)
        }catch {
            print(error)
            print(self.asString)
            return nil
        }
    }
}
